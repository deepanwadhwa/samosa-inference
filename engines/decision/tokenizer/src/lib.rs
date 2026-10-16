//! Native bridge to the exact tokenizer implementation used by the reference.
//! No model downloads, Python callbacks, or training features are enabled.
use serde::{Deserialize, Serialize};
mod request;
mod state;
use std::ffi::c_void;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::Mutex;
use tokenizers::{
    PaddingDirection, PaddingParams, PaddingStrategy, PostProcessor, Tokenizer,
    TruncationDirection, TruncationParams, TruncationStrategy,
};

const INVALID: i32 = 1;
const MODEL: i32 = 2;
const ENCODING: i32 = 3;
const PANIC: i32 = 4;
const CONTEXT: i32 = 5;
const MAX_REQUEST: usize = 1_048_576;
struct Handle(Mutex<Tokenizer>);

#[repr(C)]
pub struct Output {
    data: *const u8,
    length: usize,
    owner: *mut c_void,
}
#[derive(Clone, Copy, Deserialize)]
#[serde(rename_all = "snake_case")]
enum Truncation {
    None,
    LongestFirst,
    OnlyFirst,
    OnlySecond,
}
#[derive(Clone, Copy, Deserialize)]
#[serde(rename_all = "snake_case")]
enum Padding {
    None,
    Longest,
    MaxLength,
}
#[derive(Clone, Copy, Default, Deserialize)]
#[serde(rename_all = "snake_case")]
enum Direction {
    Left,
    #[default]
    Right,
}
fn pad_token() -> String {
    "[PAD]".into()
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    texts: Vec<Vec<String>>,
    max_length: usize,
    truncation: Truncation,
    padding: Padding,
    #[serde(default)]
    truncation_side: Direction,
    #[serde(default)]
    padding_side: Direction,
    #[serde(default)]
    pad_id: u32,
    #[serde(default)]
    pad_type_id: u32,
    #[serde(default = "pad_token")]
    pad_token: String,
}
#[derive(Serialize)]
struct Encoded {
    input_ids: Vec<Vec<u32>>,
    attention_mask: Vec<Vec<u32>>,
    token_type_ids: Vec<Vec<u32>>,
}

fn encode(handle: &Handle, bytes: &[u8]) -> Result<Vec<u8>, i32> {
    let request: Request = serde_json::from_slice(bytes).map_err(|_| INVALID)?;
    if request.texts.is_empty()
        || request.texts.len() > 8
        || request.max_length == 0
        || request.max_length > 512
        || request.pad_type_id > 1
        || request.pad_token.len() > 128
    {
        return Err(INVALID);
    }
    let mut total = 0usize;
    for input in &request.texts {
        if input.is_empty() || input.len() > 2 {
            return Err(INVALID);
        }
        for text in input {
            if text.len() > 65536 {
                return Err(INVALID);
            }
            total += text.len();
        }
    }
    if total > 524288 {
        return Err(INVALID);
    }
    let mut tokenizer = handle.0.lock().map_err(|_| ENCODING)?;
    if tokenizer.token_to_id(&request.pad_token) != Some(request.pad_id) {
        return Err(INVALID);
    }
    if !matches!(request.truncation, Truncation::None) {
        for input in &request.texts {
            let special = tokenizer
                .get_post_processor()
                .map_or(0, |p| p.added_tokens(input.len() == 2));
            if request.max_length < special {
                return Err(INVALID);
            }
        }
    }
    let truncation = match request.truncation {
        Truncation::None => None,
        strategy => Some(TruncationParams {
            max_length: request.max_length,
            stride: 0,
            direction: match request.truncation_side {
                Direction::Left => TruncationDirection::Left,
                Direction::Right => TruncationDirection::Right,
            },
            strategy: match strategy {
                Truncation::OnlyFirst => TruncationStrategy::OnlyFirst,
                Truncation::OnlySecond => TruncationStrategy::OnlySecond,
                _ => TruncationStrategy::LongestFirst,
            },
        }),
    };
    tokenizer.with_truncation(truncation).map_err(|_| INVALID)?;
    tokenizer.with_padding(match request.padding {
        Padding::None => None,
        padding => Some(PaddingParams {
            strategy: match padding {
                Padding::MaxLength => PaddingStrategy::Fixed(request.max_length),
                _ => PaddingStrategy::BatchLongest,
            },
            direction: match request.padding_side {
                Direction::Left => PaddingDirection::Left,
                Direction::Right => PaddingDirection::Right,
            },
            pad_id: request.pad_id,
            pad_type_id: request.pad_type_id,
            pad_token: request.pad_token,
            pad_to_multiple_of: None,
        }),
    });
    let inputs: Vec<tokenizers::EncodeInput<'_>> = request
        .texts
        .iter()
        .map(|input| {
            if input.len() == 2 {
                (input[0].as_str(), input[1].as_str()).into()
            } else {
                input[0].as_str().into()
            }
        })
        .collect();
    let result = tokenizer.encode_batch(inputs, true).map_err(|_| ENCODING)?;
    if result.iter().any(|value| value.len() > request.max_length) {
        return Err(CONTEXT);
    }
    let encoded = Encoded {
        input_ids: result.iter().map(|v| v.get_ids().to_vec()).collect(),
        attention_mask: result
            .iter()
            .map(|v| v.get_attention_mask().to_vec())
            .collect(),
        token_type_ids: result.iter().map(|v| v.get_type_ids().to_vec()).collect(),
    };
    let bytes = serde_json::to_vec(&encoded).map_err(|_| ENCODING)?;
    if bytes.len() > MAX_REQUEST {
        return Err(CONTEXT);
    }
    Ok(bytes)
}

/// Foreign callers must supply live buffers and valid output/handle pointers.
#[no_mangle]
pub unsafe extern "C" fn sm_tokenizer_load(
    data: *const u8,
    length: usize,
    output: *mut *mut c_void,
) -> i32 {
    if data.is_null()
        || length == 0
        || length > 16777216
        || output.is_null()
        || !(*output).is_null()
    {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        tokenizers::utils::parallelism::set_parallelism(false);
        Tokenizer::from_bytes(std::slice::from_raw_parts(data, length)).map_err(|_| MODEL)
    })) {
        Ok(Ok(tokenizer)) => {
            *output = Box::into_raw(Box::new(Handle(Mutex::new(tokenizer)))).cast();
            0
        }
        Ok(Err(code)) => code,
        Err(_) => PANIC,
    }
}

#[no_mangle]
pub unsafe extern "C" fn sm_tokenizer_encode(
    handle: *mut c_void,
    data: *const u8,
    length: usize,
    output: *mut Output,
) -> i32 {
    if handle.is_null() || data.is_null() || length == 0 || length > MAX_REQUEST || output.is_null()
    {
        return INVALID;
    }
    if !(*output).data.is_null() || (*output).length != 0 || !(*output).owner.is_null() {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        encode(
            &*handle.cast::<Handle>(),
            std::slice::from_raw_parts(data, length),
        )
    })) {
        Ok(Ok(bytes)) => {
            let owned = Box::new(bytes);
            let result = Output {
                data: owned.as_ptr(),
                length: owned.len(),
                owner: Box::into_raw(owned).cast(),
            };
            *output = result;
            0
        }
        Ok(Err(code)) => code,
        Err(_) => PANIC,
    }
}

#[no_mangle]
pub unsafe extern "C" fn sm_tokenizer_free(handle: *mut c_void) {
    if !handle.is_null() {
        drop(Box::from_raw(handle.cast::<Handle>()));
    }
}
#[no_mangle]
pub unsafe extern "C" fn sm_tokenizer_output_free(output: *mut Output) {
    if output.is_null() {
        return;
    }
    if !(*output).owner.is_null() {
        drop(Box::from_raw((*output).owner.cast::<Vec<u8>>()));
    }
    *output = Output {
        data: ptr::null(),
        length: 0,
        owner: ptr::null_mut(),
    };
}

/// Render bounded JSON state with the existing OpenDecision text convention.
#[no_mangle]
pub unsafe extern "C" fn sm_decision_state(
    data: *const u8,
    length: usize,
    output: *mut Output,
) -> i32 {
    if data.is_null() || length == 0 || length > MAX_REQUEST || output.is_null() {
        return INVALID;
    }
    if !(*output).data.is_null() || (*output).length != 0 || !(*output).owner.is_null() {
        return INVALID;
    }
    match catch_unwind(AssertUnwindSafe(|| {
        state::render(std::slice::from_raw_parts(data, length))
    })) {
        Ok(Ok(bytes)) => {
            let owned = Box::new(bytes);
            *output = Output {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn owned_encoding_and_settings_reset_through_c_abi() {
        // This deliberately tiny WordLevel fixture verifies transport and state;
        // it does not claim DeBERTa model or Unigram tokenizer qualification.
        let vocab = [("[UNK]", 0), ("[PAD]", 1), ("hello", 2), ("world", 3)]
            .into_iter()
            .map(|(name, id)| (name.to_owned(), id))
            .collect();
        let model = tokenizers::models::wordlevel::WordLevel::builder()
            .vocab(vocab)
            .unk_token("[UNK]".into())
            .build()
            .unwrap();
        let mut tokenizer = Tokenizer::new(model);
        tokenizer.with_pre_tokenizer(Some(
            tokenizers::pre_tokenizers::whitespace::Whitespace::default(),
        ));
        let bytes = tokenizer.to_string(false).unwrap();
        unsafe {
            let mut handle = ptr::null_mut();
            assert_eq!(
                sm_tokenizer_load(bytes.as_ptr(), bytes.len(), &mut handle),
                0
            );
            for padding in ["max_length", "none", "longest"] {
                let request = serde_json::json!({"texts": [["hello world"], ["hello"]],
                    "max_length": 4, "padding": padding, "truncation": "none", "pad_id": 1});
                let data = serde_json::to_vec(&request).unwrap();
                let mut output = Output {
                    data: ptr::null(),
                    length: 0,
                    owner: ptr::null_mut(),
                };
                assert_eq!(
                    sm_tokenizer_encode(handle, data.as_ptr(), data.len(), &mut output),
                    0
                );
                let actual: serde_json::Value =
                    serde_json::from_slice(std::slice::from_raw_parts(output.data, output.length))
                        .unwrap();
                let expected = match padding {
                    "max_length" => serde_json::json!([[2, 3, 1, 1], [2, 1, 1, 1]]),
                    "longest" => serde_json::json!([[2, 3], [2, 1]]),
                    _ => serde_json::json!([[2, 3], [2]]),
                };
                assert_eq!(actual["input_ids"], expected);
                let original = (output.data, output.length, output.owner);
                assert_eq!(
                    sm_tokenizer_encode(handle, data.as_ptr(), data.len(), &mut output),
                    INVALID
                );
                assert_eq!((output.data, output.length, output.owner), original);
                sm_tokenizer_output_free(&mut output);
                sm_tokenizer_output_free(&mut output);
                assert!(output.data.is_null() && output.owner.is_null() && output.length == 0);
            }
            sm_tokenizer_free(handle);
        }
    }

    // These tests exercise ABI refusal without inventing a replacement model.
    // Actual tokenizer behavior is qualified against the pinned HF wrapper.
    #[test]
    fn load_refuses_invalid_arguments_without_publishing_a_handle() {
        unsafe {
            let mut handle = ptr::null_mut();
            assert_eq!(sm_tokenizer_load(ptr::null(), 1, &mut handle), INVALID);
            assert_eq!(sm_tokenizer_load(b"{}".as_ptr(), 0, &mut handle), INVALID);
            assert_eq!(
                sm_tokenizer_load(b"{}".as_ptr(), 16777217, &mut handle),
                INVALID
            );
            assert_eq!(
                sm_tokenizer_load(b"{}".as_ptr(), 2, ptr::null_mut()),
                INVALID
            );
            assert_eq!(sm_tokenizer_load(b"{}".as_ptr(), 2, &mut handle), MODEL);
            assert!(handle.is_null());
            sm_tokenizer_free(handle);
        }
    }

    #[test]
    fn null_handle_refusal_preserves_output_and_empty_free_is_repeatable() {
        unsafe {
            let mut output = Output {
                data: ptr::null(),
                length: 0,
                owner: ptr::null_mut(),
            };
            assert_eq!(
                sm_tokenizer_encode(ptr::null_mut(), b"{}".as_ptr(), 2, &mut output),
                INVALID
            );
            assert!(output.data.is_null() && output.owner.is_null() && output.length == 0);
            sm_tokenizer_output_free(&mut output);
            sm_tokenizer_output_free(&mut output);
            sm_tokenizer_output_free(ptr::null_mut());
        }
    }
}
