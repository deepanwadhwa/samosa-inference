//! Python-compatible JSON state text without a Python runtime. RawValue keeps
//! integers exact instead of silently rounding large integer states to f64.
use crate::{CONTEXT, INVALID};
use serde_json::value::RawValue;
use std::collections::BTreeMap;

fn float_text(raw: &str) -> Result<String, i32> {
    let value: f64 = raw.parse().map_err(|_| INVALID)?;
    if !value.is_finite() {
        return Err(INVALID);
    }
    if value == 0.0 {
        return Ok(if value.is_sign_negative() {
            "-0.0"
        } else {
            "0.0"
        }
        .into());
    }
    let shortest = serde_json::to_string(&value).map_err(|_| INVALID)?;
    let negative = shortest.starts_with('-');
    let unsigned = shortest.trim_start_matches('-');
    let (mantissa, exponent) = unsigned.split_once('e').unwrap_or((unsigned, "0"));
    let decimal = mantissa.find('.').unwrap_or(mantissa.len()) as i32;
    let digits: String = mantissa.chars().filter(|c| *c != '.').collect();
    let leading = digits.bytes().take_while(|c| *c == b'0').count();
    let exponent: i32 =
        exponent.parse::<i32>().map_err(|_| INVALID)? + decimal - leading as i32 - 1;
    let significant = digits[leading..].trim_end_matches('0');
    let mut text = if negative {
        "-".to_owned()
    } else {
        String::new()
    };
    // Python repr uses fixed notation for exponents -4 through 15. JSON floats
    // retain .0 for integral values and an explicit, two-digit exponent sign.
    if !(-4..16).contains(&exponent) {
        text.push_str(&significant[..1]);
        if significant.len() > 1 {
            text.push('.');
            text.push_str(&significant[1..]);
        }
        text.push('e');
        text.push(if exponent < 0 { '-' } else { '+' });
        text.push_str(&format!("{:02}", exponent.abs()));
    } else {
        let point = exponent + 1;
        if point <= 0 {
            text.push_str("0.");
            text.push_str(&"0".repeat((-point) as usize));
            text.push_str(significant);
        } else if point as usize >= significant.len() {
            text.push_str(significant);
            text.push_str(&"0".repeat(point as usize - significant.len()));
            text.push_str(".0");
        } else {
            text.push_str(&significant[..point as usize]);
            text.push('.');
            text.push_str(&significant[point as usize..]);
        }
    }
    Ok(text)
}
fn nested(raw: &RawValue, depth: usize, output: &mut String) -> Result<(), i32> {
    if depth > 64 {
        return Err(CONTEXT);
    }
    let text = raw.get();
    match text.as_bytes()[0] {
        b'{' => {
            let map: BTreeMap<String, &RawValue> =
                serde_json::from_str(text).map_err(|_| INVALID)?;
            output.push('{');
            for (index, (key, value)) in map.iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                output.push_str(&serde_json::to_string(key).map_err(|_| INVALID)?);
                output.push(':');
                nested(value, depth + 1, output)?;
            }
            output.push('}');
        }
        b'[' => {
            let values: Vec<&RawValue> = serde_json::from_str(text).map_err(|_| INVALID)?;
            output.push('[');
            for (index, value) in values.iter().enumerate() {
                if index > 0 {
                    output.push(',');
                }
                nested(value, depth + 1, output)?;
            }
            output.push(']');
        }
        b'"' => {
            let value: String = serde_json::from_str(text).map_err(|_| INVALID)?;
            output.push_str(&serde_json::to_string(&value).map_err(|_| INVALID)?);
        }
        b't' | b'f' | b'n' => output.push_str(text),
        _ if text.contains(['.', 'e', 'E']) => output.push_str(&float_text(text)?),
        _ => output.push_str(if text == "-0" { "0" } else { text }),
    }
    if output.len() > 65536 {
        return Err(CONTEXT);
    }
    Ok(())
}
pub fn render(bytes: &[u8]) -> Result<Vec<u8>, i32> {
    let raw: &RawValue = serde_json::from_slice(bytes).map_err(|_| INVALID)?;
    let mut output = String::new();
    if raw.get().starts_with('"') {
        output = serde_json::from_str(raw.get()).map_err(|_| INVALID)?;
    } else {
        nested(raw, 0, &mut output)?;
    }
    if output.len() > 65536 {
        return Err(CONTEXT);
    }
    Ok(output.into_bytes())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn python_state_basics() {
        for (input, expected) in [
            (r#"{"z":1.0,"a":"é"}"#, r#"{"a":"é","z":1.0}"#),
            ("[1e-5,1e16,-0,-0.0]", "[1e-05,1e+16,0,-0.0]"),
            (r#""hello\u0000world""#, "hello\0world"),
            (
                "123456789012345678901234567890",
                "123456789012345678901234567890",
            ),
        ] {
            assert_eq!(render(input.as_bytes()).unwrap(), expected.as_bytes());
        }
        for bad in ["1e999", r#""\ud800""#, "[1,]", "NaN"] {
            assert!(render(bad.as_bytes()).is_err());
        }
    }
}
