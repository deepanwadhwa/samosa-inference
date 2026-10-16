//! Strict JSON decoding and borrowed C views. Public JSON never reaches the
//! C++ model's small trusted-transport parser. Candidate insertion order matters.
use crate::{state, CONTEXT, INVALID, MAX_REQUEST, PANIC};
use serde::de::{Deserialize, Deserializer, MapAccess, Visitor};
use serde_json::value::RawValue;
use std::{
    collections::BTreeSet,
    ffi::c_void,
    fmt,
    panic::{catch_unwind, AssertUnwindSafe},
};

struct Ordered<T>(Vec<(String, T)>);
impl<'de, T: Deserialize<'de>> Deserialize<'de> for Ordered<T> {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        struct Entries<T>(std::marker::PhantomData<T>);
        impl<'de, T: Deserialize<'de>> Visitor<'de> for Entries<T> {
            type Value = Ordered<T>;
            fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
                f.write_str("object with unique keys")
            }
            fn visit_map<A: MapAccess<'de>>(self, mut map: A) -> Result<Self::Value, A::Error> {
                let mut entries = Vec::new();
                let mut names = BTreeSet::new();
                while let Some((name, value)) = map.next_entry::<String, T>()? {
                    if !names.insert(name.clone()) {
                        return Err(serde::de::Error::custom("duplicate key"));
                    }
                    entries.push((name, value));
                }
                Ok(Ordered(entries))
            }
        }
        deserializer.deserialize_map(Entries(std::marker::PhantomData))
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Text {
    data: *const u8,
    length: usize,
}
impl Text {
    fn of(value: &str) -> Self {
        Self {
            data: value.as_ptr(),
            length: value.len(),
        }
    }
}
#[repr(C)]
pub struct Candidate {
    name: Text,
    description: Text,
}
#[repr(C)]
pub struct Relation {
    state: Text,
    proposition: Text,
    contradiction: Text,
}
#[repr(C)]
pub struct View {
    kind: u32,
    state: Text,
    instructions: Text,
    candidates: *const Candidate,
    candidate_count: usize,
    levels: *const Text,
    level_count: usize,
    relations: *const Relation,
    relation_count: usize,
    threshold: f64,
}
struct Owned {
    kind: u32,
    state: String,
    instructions: String,
    candidates: Vec<(String, String)>,
    levels: Vec<String>,
    relations: Vec<(String, String, String)>,
    candidate_views: Vec<Candidate>,
    level_views: Vec<Text>,
    relation_views: Vec<Relation>,
    threshold: f64,
}
type Fields<'a> = Vec<(String, &'a RawValue)>;
fn fields(raw: &RawValue) -> Result<Fields<'_>, i32> {
    serde_json::from_str::<Ordered<&RawValue>>(raw.get())
        .map(|v| v.0)
        .map_err(|_| INVALID)
}
fn field<'a>(values: &Fields<'a>, name: &str) -> Result<&'a RawValue, i32> {
    values
        .iter()
        .find(|(key, _)| key == name)
        .map(|(_, value)| *value)
        .ok_or(INVALID)
}
fn allowed(values: &Fields<'_>, names: &[&str]) -> Result<(), i32> {
    if values.iter().any(|(key, _)| !names.contains(&key.as_str())) {
        Err(INVALID)
    } else {
        Ok(())
    }
}
fn string(raw: &RawValue) -> Result<String, i32> {
    let value: String = serde_json::from_str(raw.get()).map_err(|_| INVALID)?;
    if value.len() > 65536 {
        return Err(CONTEXT);
    }
    Ok(value)
}
fn state_text(raw: &RawValue) -> Result<String, i32> {
    String::from_utf8(state::render(raw.get().as_bytes())?).map_err(|_| INVALID)
}
fn relation(raw: &RawValue) -> Result<(String, String, String), i32> {
    let values = fields(raw)?;
    allowed(&values, &["state", "proposition", "contradiction"])?;
    Ok((
        state_text(field(&values, "state")?)?,
        string(field(&values, "proposition")?)?,
        string(field(&values, "contradiction")?)?,
    ))
}
fn decode(bytes: &[u8]) -> Result<Owned, i32> {
    let raw: &RawValue = serde_json::from_slice(bytes).map_err(|_| INVALID)?;
    let values = fields(raw)?;
    let method = string(field(&values, "method")?)?;
    let mut request = Owned {
        kind: 0,
        state: String::new(),
        instructions: String::new(),
        candidates: Vec::new(),
        levels: Vec::new(),
        relations: Vec::new(),
        candidate_views: Vec::new(),
        level_views: Vec::new(),
        relation_views: Vec::new(),
        threshold: 0.5,
    };
    match method.as_str() {
        "choice" | "score" | "noul" => {
            allowed(&values, &["method", "state", "instructions", "criteria"])?;
            request.state = state_text(field(&values, "state")?)?;
            request.instructions = string(field(&values, "instructions")?)?;
            if method == "choice" {
                request.kind = 1;
                let criteria = fields(field(&values, "criteria")?)?;
                if !(2..=128).contains(&criteria.len()) {
                    return Err(INVALID);
                }
                for (name, value) in criteria {
                    if name.len() > 65536 {
                        return Err(CONTEXT);
                    }
                    let description = if value.get() == "null" {
                        String::new()
                    } else {
                        string(value)?
                    };
                    request.candidates.push((name, description));
                }
            } else if method == "score" {
                request.kind = 2;
                let levels: Vec<&RawValue> =
                    serde_json::from_str(field(&values, "criteria")?.get()).map_err(|_| INVALID)?;
                if !(2..=128).contains(&levels.len()) {
                    return Err(INVALID);
                }
                let mut unique = BTreeSet::new();
                for raw in levels {
                    let level = string(raw)?;
                    if !unique.insert(level.clone()) {
                        return Err(INVALID);
                    }
                    request.levels.push(level);
                }
            } else {
                request.kind = 3;
                if let Ok(criteria) = field(&values, "criteria") {
                    if criteria.get() != "null" {
                        let criteria = fields(criteria)?;
                        allowed(&criteria, &["true", "false"])?;
                        request.kind = 4;
                        request.levels = vec![
                            string(field(&criteria, "true")?)?,
                            string(field(&criteria, "false")?)?,
                        ];
                    }
                }
            }
        }
        "relation" | "relations" => {
            if method == "relation" {
                request.kind = 5;
                allowed(
                    &values,
                    &[
                        "method",
                        "state",
                        "proposition",
                        "contradiction",
                        "threshold",
                    ],
                )?;
                request.relations.push((
                    state_text(field(&values, "state")?)?,
                    string(field(&values, "proposition")?)?,
                    string(field(&values, "contradiction")?)?,
                ));
            } else {
                request.kind = 6;
                allowed(&values, &["method", "items", "threshold"])?;
                let items: Vec<&RawValue> =
                    serde_json::from_str(field(&values, "items")?.get()).map_err(|_| INVALID)?;
                if items.len() > 64 {
                    return Err(CONTEXT);
                }
                for item in items {
                    request.relations.push(relation(item)?);
                }
            }
            if let Ok(value) = field(&values, "threshold") {
                request.threshold = serde_json::from_str(value.get()).map_err(|_| INVALID)?;
                if !request.threshold.is_finite()
                    || request.threshold <= 0.0
                    || request.threshold >= 1.0
                {
                    return Err(INVALID);
                }
            }
        }
        _ => return Err(INVALID),
    }
    request.candidate_views = request
        .candidates
        .iter()
        .map(|(name, description)| Candidate {
            name: Text::of(name),
            description: Text::of(description),
        })
        .collect();
    request.level_views = request.levels.iter().map(|value| Text::of(value)).collect();
    request.relation_views = request
        .relations
        .iter()
        .map(|(state, proposition, contradiction)| Relation {
            state: Text::of(state),
            proposition: Text::of(proposition),
            contradiction: Text::of(contradiction),
        })
        .collect();
    Ok(request)
}
// Normalize only after strict decoding. Criteria order remains part of request
// identity; state objects have already been rendered with Python semantics.
fn normalized(request: &Owned) -> Result<Vec<u8>, i32> {
    use serde::ser::{SerializeMap, Serializer};
    struct Criteria<'a>(&'a [(String, String)]);
    impl serde::Serialize for Criteria<'_> {
        fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
            let mut map = serializer.serialize_map(Some(self.0.len()))?;
            for (name, description) in self.0 {
                map.serialize_entry(name, description)?;
            }
            map.end()
        }
    }
    #[derive(serde::Serialize)]
    struct Item<'a> {
        state: &'a str,
        proposition: &'a str,
        contradiction: &'a str,
    }
    let mut bytes = Vec::new();
    let mut serializer = serde_json::Serializer::new(&mut bytes);
    let mut map = serializer.serialize_map(None).map_err(|_| INVALID)?;
    let method = match request.kind {
        1 => "choice",
        2 => "score",
        3 | 4 => "noul",
        5 => "relation",
        6 => "relations",
        _ => return Err(INVALID),
    };
    map.serialize_entry("method", method).map_err(|_| INVALID)?;
    if request.kind <= 4 {
        map.serialize_entry("state", &request.state)
            .map_err(|_| INVALID)?;
        map.serialize_entry("instructions", &request.instructions)
            .map_err(|_| INVALID)?;
        match request.kind {
            1 => map.serialize_entry("criteria", &Criteria(&request.candidates)),
            2 => map.serialize_entry("criteria", &request.levels),
            4 => map.serialize_entry(
                "criteria",
                &Criteria(&[
                    ("true".into(), request.levels[0].clone()),
                    ("false".into(), request.levels[1].clone()),
                ]),
            ),
            _ => Ok(()),
        }
        .map_err(|_| INVALID)?;
    } else {
        if request.kind == 5 {
            let (state, proposition, contradiction) = &request.relations[0];
            map.serialize_entry("state", state).map_err(|_| INVALID)?;
            map.serialize_entry("proposition", proposition)
                .map_err(|_| INVALID)?;
            map.serialize_entry("contradiction", contradiction)
                .map_err(|_| INVALID)?;
        } else {
            let items: Vec<_> = request
                .relations
                .iter()
                .map(|(state, proposition, contradiction)| Item {
                    state,
                    proposition,
                    contradiction,
                })
                .collect();
            map.serialize_entry("items", &items).map_err(|_| INVALID)?;
        }
        map.serialize_entry("threshold", &request.threshold)
            .map_err(|_| INVALID)?;
    }
    map.end().map_err(|_| INVALID)?;
    if bytes.len() > MAX_REQUEST {
        return Err(CONTEXT);
    }
    Ok(bytes)
}
/// Owned worker payload for host admission and operation identity. No model or
/// tokenizer is loaded. Output ownership matches sm_tokenizer_output_free.
#[no_mangle]
pub unsafe extern "C" fn sm_decision_request_normalized(
    handle: *const c_void,
    output: *mut crate::Output,
) -> i32 {
    if handle.is_null() || output.is_null() {
        return INVALID;
    }
    if !(*output).data.is_null() || (*output).length != 0 || !(*output).owner.is_null() {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| normalized(&*handle.cast::<Owned>()))) {
        Ok(Ok(bytes)) => {
            let owned = Box::new(bytes);
            *output = crate::Output {
                data: owned.as_ptr(),
                length: owned.len(),
                owner: Box::into_raw(owned).cast(),
            };
            0
        }
        Ok(Err(code)) => code,
        Err(_) => PANIC,
    }
}
// Validate typed results against the exact admitted request before publishing.
// This is model-free shape/range/consistency validation, not model qualification.
fn result_number(raw: &RawValue) -> Result<f64, i32> {
    let value: f64 = serde_json::from_str(raw.get()).map_err(|_| INVALID)?;
    if value.is_finite() {
        Ok(value)
    } else {
        Err(INVALID)
    }
}
fn probability(raw: &RawValue) -> Result<f64, i32> {
    let value = result_number(raw)?;
    if (0.0..=1.0).contains(&value) {
        Ok(value)
    } else {
        Err(INVALID)
    }
}
fn exact_fields<'a>(raw: &'a RawValue, names: &[&str]) -> Result<Fields<'a>, i32> {
    let values = fields(raw)?;
    allowed(&values, names)?;
    if values.len() != names.len() {
        return Err(INVALID);
    }
    Ok(values)
}
fn text_is(values: &Fields<'_>, name: &str, expected: &str) -> Result<(), i32> {
    if string(field(values, name)?)? == expected {
        Ok(())
    } else {
        Err(INVALID)
    }
}
fn result_relation(raw: &RawValue, threshold: f64, backend: &str) -> Result<(), i32> {
    let values = exact_fields(raw, &["type", "relation", "scores", "threshold", "backend"])?;
    text_is(&values, "type", "relation")?;
    text_is(&values, "backend", backend)?;
    if result_number(field(&values, "threshold")?)? != threshold {
        return Err(INVALID);
    }
    let scores = exact_fields(field(&values, "scores")?, &["supports", "contradicts"])?;
    let support = probability(field(&scores, "supports")?)? >= threshold;
    let contradict = probability(field(&scores, "contradicts")?)? >= threshold;
    let expected = match (support, contradict) {
        (true, true) => "conflicted",
        (true, false) => "supports",
        (false, true) => "contradicts",
        (false, false) => "unknown",
    };
    text_is(&values, "relation", expected)
}
#[cfg(test)]
fn validate_result(request: &Owned, bytes: &[u8]) -> Result<(), i32> {
    validate_result_profile(request, bytes, 0)
}
fn validate_result_profile(request: &Owned, bytes: &[u8], profile: u32) -> Result<(), i32> {
    let backend = match profile {
        0 => "explicit_opposite",
        1 if request.kind != 6 => "siglip_image_text",
        _ => return Err(INVALID),
    };
    let raw: &RawValue = serde_json::from_slice(bytes).map_err(|_| INVALID)?;
    match request.kind {
        1 | 2 => {
            let names: &[&str] = if request.kind == 1 {
                &["type", "choice", "probabilities", "confidence"]
            } else {
                &["type", "score", "legend", "probabilities", "confidence"]
            };
            let values = exact_fields(raw, names)?;
            text_is(
                &values,
                "type",
                if request.kind == 1 { "choice" } else { "score" },
            )?;
            let distribution = fields(field(&values, "probabilities")?)?;
            let expected: Vec<String> = if request.kind == 1 {
                request
                    .candidates
                    .iter()
                    .map(|(name, _)| name.clone())
                    .collect()
            } else {
                (0..request.levels.len())
                    .map(|index| index.to_string())
                    .collect()
            };
            if distribution.len() != expected.len() {
                return Err(INVALID);
            }
            let mut probabilities = Vec::new();
            for ((name, value), expected) in distribution.iter().zip(&expected) {
                if name != expected {
                    return Err(INVALID);
                }
                probabilities.push(probability(value)?);
            }
            if (probabilities.iter().sum::<f64>() - 1.0).abs() > 1e-5 {
                return Err(INVALID);
            }
            let entropy: f64 = probabilities
                .iter()
                .filter(|p| **p > 0.0)
                .map(|p| -p * p.ln())
                .sum();
            let confidence = (1.0 - entropy / (probabilities.len() as f64).ln()).clamp(0.0, 1.0);
            if (probability(field(&values, "confidence")?)? - confidence).abs() > 1e-5 {
                return Err(INVALID);
            }
            if request.kind == 1 {
                let mut selected = 0;
                for i in 1..probabilities.len() {
                    if probabilities[i] > probabilities[selected] {
                        selected = i;
                    }
                }
                text_is(&values, "choice", &expected[selected])?;
            } else {
                let legend = fields(field(&values, "legend")?)?;
                if legend.len() != expected.len() {
                    return Err(INVALID);
                }
                for (i, (name, description)) in legend.iter().enumerate() {
                    if name != &expected[i] || string(description)? != request.levels[i] {
                        return Err(INVALID);
                    }
                }
                let score: f64 = probabilities
                    .iter()
                    .enumerate()
                    .map(|(i, p)| i as f64 * p)
                    .sum();
                if (result_number(field(&values, "score")?)? - score).abs() > 1e-5 {
                    return Err(INVALID);
                }
            }
            Ok(())
        }
        3 | 4 => {
            let values = exact_fields(raw, &["type", "noul"])?;
            text_is(&values, "type", "noul")?;
            probability(field(&values, "noul")?).map(|_| ())
        }
        5 => result_relation(raw, request.threshold, backend),
        6 => {
            let results: Vec<&RawValue> = serde_json::from_str(raw.get()).map_err(|_| INVALID)?;
            if results.len() != request.relations.len() {
                return Err(INVALID);
            }
            for result in results {
                result_relation(result, request.threshold, backend)?;
            }
            Ok(())
        }
        _ => Err(INVALID),
    }
}
#[no_mangle]
pub unsafe extern "C" fn sm_decision_result_validate(
    handle: *const c_void,
    data: *const u8,
    length: usize,
) -> i32 {
    sm_decision_result_validate_profile(handle, data, length, 0)
}
#[no_mangle]
pub unsafe extern "C" fn sm_decision_result_validate_profile(
    handle: *const c_void,
    data: *const u8,
    length: usize,
    profile: u32,
) -> i32 {
    if handle.is_null() || data.is_null() || length == 0 || length > MAX_REQUEST {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        validate_result_profile(
            &*handle.cast::<Owned>(),
            std::slice::from_raw_parts(data, length),
            profile,
        )
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(code)) => code,
        Err(_) => PANIC,
    }
}
#[no_mangle]
pub unsafe extern "C" fn sm_decision_request_load(
    data: *const u8,
    length: usize,
    output: *mut *mut c_void,
) -> i32 {
    if data.is_null()
        || length == 0
        || length > MAX_REQUEST
        || output.is_null()
        || !(*output).is_null()
    {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        decode(std::slice::from_raw_parts(data, length))
    })) {
        Ok(Ok(request)) => {
            *output = Box::into_raw(Box::new(request)).cast();
            0
        }
        Ok(Err(code)) => code,
        Err(_) => PANIC,
    }
}
#[no_mangle]
pub unsafe extern "C" fn sm_decision_request_view(handle: *const c_void, output: *mut View) -> i32 {
    if handle.is_null() || output.is_null() {
        return INVALID;
    }
    let request = &*handle.cast::<Owned>();
    *output = View {
        kind: request.kind,
        state: Text::of(&request.state),
        instructions: Text::of(&request.instructions),
        candidates: request.candidate_views.as_ptr(),
        candidate_count: request.candidate_views.len(),
        levels: request.level_views.as_ptr(),
        level_count: request.level_views.len(),
        relations: request.relation_views.as_ptr(),
        relation_count: request.relation_views.len(),
        threshold: request.threshold,
    };
    0
}
#[no_mangle]
pub unsafe extern "C" fn sm_decision_request_free(handle: *mut c_void) {
    if !handle.is_null() {
        drop(Box::from_raw(handle.cast::<Owned>()));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn preserve_candidate_order_and_render_state() {
        let bytes=br#"{"method":"choice","state":{"z":1,"a":true},"instructions":"Ask\u0000now","criteria":{"z":"Last","a":null}}"#;
        let request = decode(bytes).unwrap();
        assert_eq!(request.kind, 1);
        assert_eq!(request.state, r#"{"a":true,"z":1}"#);
        assert_eq!(request.instructions, "Ask\0now");
        assert_eq!(
            request.candidates,
            vec![("z".into(), "Last".into()), ("a".into(), "".into())]
        );
        unsafe {
            let mut handle = std::ptr::null_mut();
            assert_eq!(
                sm_decision_request_load(bytes.as_ptr(), bytes.len(), &mut handle),
                0
            );
            let mut view: View = std::mem::zeroed();
            assert_eq!(sm_decision_request_view(handle, &mut view), 0);
            assert_eq!(view.candidate_count, 2);
            assert_eq!(
                std::slice::from_raw_parts(
                    (*view.candidates).name.data,
                    (*view.candidates).name.length
                ),
                b"z"
            );
            assert_eq!(
                std::slice::from_raw_parts(view.instructions.data, view.instructions.length),
                b"Ask\0now"
            );
            assert_eq!(
                sm_decision_request_load(bytes.as_ptr(), bytes.len(), &mut handle),
                INVALID
            );
            sm_decision_request_free(handle);
            sm_decision_request_free(std::ptr::null_mut());
        }
    }
    #[test]
    fn normalized_worker_payload_is_stable_and_order_sensitive() {
        for input in [
            r#"{"method":"choice","state":{"z":1,"a":true},"instructions":"q\u0000x","criteria":{"z":null,"a":"first"}}"#,
            r#"{"method":"score","state":[1,true],"instructions":"q","criteria":["low","high"]}"#,
            r#"{"method":"noul","state":null,"instructions":"q","criteria":null}"#,
            r#"{"method":"noul","state":0,"instructions":"q","criteria":{"false":"no","true":"yes"}}"#,
            r#"{"method":"relation","state":{"x":2},"proposition":"p","contradiction":"c"}"#,
            r#"{"method":"relations","items":[{"state":[0],"proposition":"p","contradiction":"c"}],"threshold":0.75}"#,
            r#"{"method":"relations","items":[]}"#,
        ] {
            let first = normalized(&decode(input.as_bytes()).unwrap()).unwrap();
            let second = normalized(&decode(&first).unwrap()).unwrap();
            assert_eq!(first, second, "{input}");
        }
        let a = decode(
            br#"{"method":"choice","state":0,"instructions":"q","criteria":{"z":"z","a":"a"}}"#,
        )
        .unwrap();
        let b = decode(
            br#"{"method":"choice","state":0,"instructions":"q","criteria":{"a":"a","z":"z"}}"#,
        )
        .unwrap();
        assert_ne!(normalized(&a).unwrap(), normalized(&b).unwrap());
        unsafe {
            let handle = Box::into_raw(Box::new(a)).cast();
            let mut output: crate::Output = std::mem::zeroed();
            assert_eq!(
                sm_decision_request_normalized(std::ptr::null(), &mut output),
                INVALID
            );
            assert!(output.owner.is_null());
            assert_eq!(sm_decision_request_normalized(handle, &mut output), 0);
            let before = (output.data, output.length, output.owner);
            assert_eq!(sm_decision_request_normalized(handle, &mut output), INVALID);
            assert_eq!(before, (output.data, output.length, output.owner));
            // The normalized payload owns its bytes independently of the handle.
            sm_decision_request_free(handle);
            let decoded = decode(std::slice::from_raw_parts(output.data, output.length)).unwrap();
            assert_eq!(decoded.candidates[0].0, "z");
            crate::sm_tokenizer_output_free(&mut output);
            crate::sm_tokenizer_output_free(&mut output);
        }
    }
    #[test]
    fn typed_results_match_the_admitted_request() {
        let choice = decode(br#"{"method":"choice","state":0,"instructions":"q","criteria":{"z":"last","a":"first"}}"#).unwrap();
        let valid =
            br#"{"type":"choice","choice":"z","probabilities":{"z":0.5,"a":0.5},"confidence":0}"#;
        assert!(validate_result(&choice, valid).is_ok());
        assert!(validate_result_profile(&choice, valid, 1).is_ok());
        assert!(validate_result_profile(&choice, valid, 2).is_err());
        for invalid in [
            r#"{"type":"choice","choice":"a","probabilities":{"z":0.5,"a":0.5},"confidence":0}"#,
            r#"{"type":"choice","choice":"z","probabilities":{"a":0.5,"z":0.5},"confidence":0}"#,
            r#"{"type":"choice","choice":"z","probabilities":{"z":0.9,"a":0.5},"confidence":0}"#,
            r#"{"type":"choice","choice":"z","probabilities":{"z":1,"a":0},"confidence":0}"#,
            r#"{"type":"choice","choice":"z","probabilities":{"z":0.5,"z":0.5},"confidence":0}"#,
            r#"{"type":"choice","choice":"z","probabilities":{"z":0.5,"a":0.5},"confidence":0,"extra":1}"#,
            r#"{"type":"noul","noul":0.5}"#,
        ] {
            assert!(
                validate_result(&choice, invalid.as_bytes()).is_err(),
                "{invalid}"
            );
            assert!(validate_result_profile(&choice, invalid.as_bytes(), 1).is_err());
        }
        let score =
            decode(br#"{"method":"score","state":0,"instructions":"q","criteria":["low","high"]}"#)
                .unwrap();
        let valid = br#"{"type":"score","score":0.5,"legend":{"0":"low","1":"high"},"probabilities":{"0":0.5,"1":0.5},"confidence":0}"#;
        assert!(validate_result(&score, valid).is_ok());
        assert!(validate_result_profile(&score, valid, 1).is_ok());
        let wrong_score = String::from_utf8(valid.to_vec())
            .unwrap()
            .replace("\"score\":0.5", "\"score\":0.8");
        assert!(validate_result(&score, wrong_score.as_bytes()).is_err());
        let wrong_legend = String::from_utf8(valid.to_vec())
            .unwrap()
            .replace("low", "changed");
        assert!(validate_result(&score, wrong_legend.as_bytes()).is_err());
        let noul = decode(br#"{"method":"noul","state":0,"instructions":"q"}"#).unwrap();
        assert!(validate_result(&noul, br#"{"type":"noul","noul":0.5}"#).is_ok());
        assert!(validate_result_profile(&noul, br#"{"type":"noul","noul":0.5}"#, 1).is_ok());
        for invalid in [
            r#"{"type":"noul","noul":-0.1}"#,
            r#"{"type":"noul","noul":1e999}"#,
            r#"{"type":"noul","noul":"0.5"}"#,
        ] {
            assert!(validate_result(&noul, invalid.as_bytes()).is_err());
        }
        let relation =
            decode(br#"{"method":"relation","state":0,"proposition":"p","contradiction":"c"}"#)
                .unwrap();
        let valid = br#"{"type":"relation","relation":"supports","scores":{"supports":0.7,"contradicts":0.2},"threshold":0.5,"backend":"explicit_opposite"}"#;
        assert!(validate_result(&relation, valid).is_ok());
        assert!(validate_result_profile(&relation, valid, 1).is_err());
        let image = String::from_utf8(valid.to_vec())
            .unwrap()
            .replace("explicit_opposite", "siglip_image_text");
        assert!(validate_result_profile(&relation, image.as_bytes(), 1).is_ok());
        assert!(validate_result(&relation, image.as_bytes()).is_err());
        let invalid_image = image.replace("\"supports\",", "\"unknown\",");
        assert!(validate_result_profile(&relation, invalid_image.as_bytes(), 1).is_err());
        let wrong_relation = String::from_utf8(valid.to_vec())
            .unwrap()
            .replace("\"supports\",", "\"conflicted\",");
        assert!(validate_result(&relation, wrong_relation.as_bytes()).is_err());
        let relations = decode(br#"{"method":"relations","items":[]}"#).unwrap();
        assert!(validate_result(&relations, b"[]").is_ok());
        assert!(validate_result_profile(&relations, b"[]", 1).is_err());
        assert!(validate_result(&relations, valid).is_err());
        unsafe {
            let handle = Box::into_raw(Box::new(relation)).cast();
            assert_eq!(
                sm_decision_result_validate(handle, valid.as_ptr(), valid.len()),
                0
            );
            assert_eq!(
                sm_decision_result_validate_profile(handle, image.as_ptr(), image.len(), 1),
                0
            );
            assert_eq!(
                sm_decision_result_validate_profile(handle, image.as_ptr(), image.len(), 0),
                INVALID
            );
            assert_eq!(
                sm_decision_result_validate_profile(handle, image.as_ptr(), image.len(), 99),
                INVALID
            );
            assert_eq!(
                sm_decision_result_validate(std::ptr::null(), valid.as_ptr(), valid.len()),
                INVALID
            );
            assert_eq!(
                sm_decision_result_validate(handle, std::ptr::null(), 0),
                INVALID
            );
            sm_decision_request_free(handle);
        }
    }
    #[test]
    fn strict_typed_refusal_and_all_operations() {
        for input in [
            r#"{"method":"score","state":null,"instructions":"q","criteria":["low","high"]}"#,
            r#"{"method":"noul","state":[],"instructions":"q"}"#,
            r#"{"method":"noul","state":[],"instructions":"q","criteria":{"true":"yes","false":"no"}}"#,
            r#"{"method":"relation","state":{},"proposition":"p","contradiction":"c"}"#,
            r#"{"method":"relations","items":[],"threshold":0.75}"#,
        ] {
            assert!(decode(input.as_bytes()).is_ok(), "{input}");
        }
        for input in [
            r#"{"method":"noul","method":"score","state":0,"instructions":"q"}"#,
            r#"{"method":"noul","state":0,"instructions":"q","extra":0}"#,
            r#"{"method":"noul","state":0,"instructions":"q","criteria":{"true":"yes"}}"#,
            r#"{"method":"choice","state":0,"instructions":"q","criteria":{"a":"one","a":"two"}}"#,
            r#"{"method":"score","state":0,"instructions":"q","criteria":["same","same"]}"#,
            r#"{"method":"relation","state":0,"proposition":"p","contradiction":"c","threshold":1}"#,
            r#"{"method":"relations","items":[{"state":0,"proposition":1,"contradiction":"c"}]}"#,
            r#"{"method":"noul","instructions":"q"}"#,
        ] {
            assert!(decode(input.as_bytes()).is_err(), "{input}");
        }
    }
}
