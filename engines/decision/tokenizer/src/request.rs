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
