#!/usr/bin/env python3
"""Export the pinned text reference for native ONNX execution (QA/build only)."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import shutil

from opendecision_reference import inventory


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(65536), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--reference-sha256", required=True)
    args = parser.parse_args()
    model, reference, output = args.model.resolve(), args.reference.resolve(), args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError("Export output must not already exist")
    if digest(reference) != args.reference_sha256:
        raise ValueError("Reference report pin mismatch")
    report = json.loads(reference.read_text())
    assets = json.loads(Path(__file__).with_name("opendecision_text_assets.json").read_text())
    if report["model_assets"] != assets or report["weights_dtype"] != "torch.float16":
        raise ValueError("Unqualified reference model profile")
    inventory(model, assets["files"])
    tokenizer = reference.with_name(report["tokenizer"]["file"])
    if tokenizer.stat().st_size != report["tokenizer"]["bytes"] or digest(tokenizer) != report["tokenizer"]["sha256"]:
        raise ValueError("Effective tokenizer pin mismatch")
    for name, version in report["versions"].items():
        if importlib.metadata.version(name) != version:
            raise ValueError("Reference dependency changed: " + name)
    if importlib.metadata.version("onnx") != "1.18.0":
        raise ValueError("Unqualified ONNX exporter")
    os.environ["HF_HUB_OFFLINE"] = "1"
    os.environ["TRANSFORMERS_OFFLINE"] = "1"
    os.environ["USE_TF"] = "0"
    import onnx
    import torch
    from transformers import AutoModelForSequenceClassification

    torch.set_num_threads(4)
    torch.manual_seed(0)
    classifier = AutoModelForSequenceClassification.from_pretrained(
        str(model), local_files_only=True, dtype=torch.float16
    ).cpu().eval()
    if classifier.config.label2id != report["label2id"] or classifier.config.type_vocab_size:
        raise ValueError("Unexpected NLI label or segment-embedding profile")

    class Logits(torch.nn.Module):
        def __init__(self, inner):
            super().__init__()
            self.inner = inner

        def forward(self, input_ids, attention_mask):
            # This checkpoint has no segment embeddings; the original forward
            # ignores token_type_ids. Verify that against every reference batch.
            return self.inner(input_ids=input_ids, attention_mask=attention_mask).logits

    wrapper = Logits(classifier).eval()
    batches = []
    with torch.no_grad():
        for case in report["cases"]:
            for batch in case["batches"]:
                inputs = tuple(torch.tensor(batch["inputs"][key], dtype=torch.int64)
                               for key in ["input_ids", "attention_mask"])
                if inputs[0].dim() != 2 or inputs[0].shape != inputs[1].shape:
                    raise ValueError("Invalid reference tensor shape")
                rows, columns = inputs[0].shape
                if not 1 <= rows <= 8 or not 1 <= columns <= 512:
                    raise ValueError("Reference tensor exceeds profile bounds")
                logits = wrapper(*inputs).float()
                expected = torch.tensor(batch["logits"], dtype=torch.float32)
                if not torch.equal(logits, expected):
                    raise ValueError("Direct model differs from reference: " + case["name"])
                batches.append(inputs)
    print(f"PASS original model tensor parity: {len(batches)} batches", flush=True)
    output.mkdir(mode=0o700)
    destination = output / "model.onnx"
    print("BEGIN pinned float16 ONNX export", flush=True)
    # The legacy exporter is explicit: dynamic batch/sequence axes must survive
    # tracing, and native execution must qualify every captured shape afterward.
    torch.onnx.export(wrapper, batches[0], str(destination), dynamo=False,
                      input_names=["input_ids", "attention_mask"], output_names=["logits"],
                      dynamic_axes={"input_ids": {0: "batch", 1: "tokens"},
                                    "attention_mask": {0: "batch", 1: "tokens"},
                                    "logits": {0: "batch"}},
                      opset_version=17, do_constant_folding=True)
    onnx.checker.check_model(str(destination), full_check=True)
    inventory(model, assets["files"])
    shutil.copyfile(tokenizer, output / "tokenizer.json")
    files = {path.name: dict(bytes=path.stat().st_size, sha256=digest(path))
             for path in sorted(output.iterdir()) if path.is_file()}
    provenance = dict(format="samosa.inference.decision-onnx-candidate.v1",
                      qualification="candidate; native execution not yet qualified",
                      source_assets=assets, reference_sha256=args.reference_sha256,
                      dtype="float16", label2id=report["label2id"], max_positions=512,
                      versions={**report["versions"], "onnx": importlib.metadata.version("onnx"),
                                "protobuf": importlib.metadata.version("protobuf")}, files=files)
    (output / "provenance.json").write_text(json.dumps(provenance, indent=2, sort_keys=True) + "\n")
    print("PASS pinned ONNX export candidate: " + str(output), flush=True)


if __name__ == "__main__":
    main()
