#!/usr/bin/env python3
"""Capture the existing OpenDecision text computation for native-backend parity.

QA only: run with the existing OpenDecision Python environment and explicitly
pinned local checkpoint. Neither Python nor this reference ships as a provider.
"""
import argparse
import hashlib
import importlib.metadata
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


REFERENCE_REVISION = "cbd0b50db42bba6975f342de1789fb3c8f2dc670"


def inventory(directory, expected):
    if {path.name for path in directory.iterdir()} != set(expected):
        raise ValueError("Unexpected reference checkpoint files")
    observed = {}
    for name, entry in expected.items():
        path = directory / name
        checksum = hashlib.sha256()
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(65536), b""):
                checksum.update(chunk)
        observed[name] = {"bytes": path.stat().st_size, "sha256": checksum.hexdigest()}
        if observed[name] != entry:
            raise ValueError(f"Reference checkpoint changed: {name}")
    return observed


def finite(value):
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError("Non-finite reference result")
    if isinstance(value, dict):
        for item in value.values():
            finite(item)
    elif isinstance(value, list):
        for item in value:
            finite(item)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("model", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    repository, model = args.repository.resolve(), args.model.resolve()
    output = args.output.absolute()
    tokenizer_output = output.with_suffix(".tokenizer.json")
    if output.exists() or tokenizer_output.exists():
        raise ValueError("Reference output must not already exist")
    assets = json.loads(Path(__file__).with_name("opendecision_text_assets.json").read_text())
    inventory(model, assets["files"])
    revision = subprocess.check_output(
        ["git", "-C", str(repository), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != REFERENCE_REVISION:
        raise ValueError("Unqualified OpenDecision reference revision")

    # Import an immutable tracked-source export rather than an editable package,
    # an installed wheel, or ignored bytecode from the original checkout.
    with tempfile.TemporaryDirectory(prefix="samosa-decision-reference-") as temporary:
        snapshot = Path(temporary)
        archive = snapshot / "reference.tar"
        subprocess.run(["git", "-C", str(repository), "archive", "--format=tar",
                        "--output=" + str(archive), "HEAD", "src/opendecision", "LICENSE"], check=True)
        with tarfile.open(archive) as source:
            source.extractall(snapshot, filter="data")
        sys.path.insert(0, str(snapshot / "src"))
        os.environ["HF_HUB_OFFLINE"] = "1"
        os.environ["TRANSFORMERS_OFFLINE"] = "1"
        os.environ["USE_TF"] = "0"
        import torch
        from opendecision import OpenDecisionEngine

        torch.set_num_threads(4)
        torch.manual_seed(0)
        engine = OpenDecisionEngine(model=str(model), device=-1, batch_size=8)
        classifier = engine.classifier.model
        # Transformers can configure its backend beyond the cached JSON. Save
        # the effective initializer before per-call padding/truncation mutates it.
        tokenizer_bytes = engine.classifier.tokenizer.backend_tokenizer.to_str().encode("utf-8")
        batches = []

        # Observe actual post-tokenization tensors and logits without replacing
        # the tokenizer, pipeline, scoring functions, or model forward method.
        def before_forward(module, positional, keyword):
            inputs = {name: value.detach().cpu().tolist()
                      for name, value in keyword.items() if isinstance(value, torch.Tensor)}
            if positional or "input_ids" not in inputs:
                raise ValueError("Unexpected reference model call")
            batches.append({"inputs": inputs})

        def after_forward(module, positional, result):
            batches[-1]["logits"] = result.logits.detach().float().cpu().tolist()

        pre = classifier.register_forward_pre_hook(before_forward, with_kwargs=True)
        post = classifier.register_forward_hook(after_forward)
        cases = [
            ("chat_route", "choice", dict(
                state="USER: Find the invoice I received yesterday.",
                instructions="Which file action does the user want?",
                criteria={"find": "Find files that match the user's description.",
                          "duplicates": "Find files with identical content.",
                          "report": "Count files and summarize what is in the folder.",
                          "sort": "Prepare moves that group files by file type.",
                          "classify": "Label files by category without moving them.",
                          "watch": "Watch this folder for future additions or changes."})),
            ("description_collision", "choice", dict(
                state="The document is an invoice.", instructions="Which kind of document is this?",
                criteria={"billing_record": "An invoice.", "purchase_note": "An invoice.",
                          "recipe": "A recipe."})),
            ("predicate_true", "noul", dict(state="The invoice total is $42.00.",
                instructions="This document describes an invoice.")),
            ("predicate_false", "noul", dict(state="The invoice total is $42.00.",
                instructions="This document is a recipe.")),
            ("explicit_binary", "noul", dict(state="Two invoices were found.",
                instructions="Is there at least one invoice?",
                criteria={"true": "At least one invoice was found.", "false": "No invoices were found."})),
            ("ordered_score", "score", dict(state="One invoice was found.",
                instructions="How many invoices were found?",
                criteria=["No invoices were found.", "One invoice was found.", "Many invoices were found."])),
            ("relation_support", "relation", dict(state="Two invoices were found.",
                proposition="Invoices were found.", contradiction="No invoices were found.", threshold=0.5)),
            ("relation_contradict", "relation", dict(state="No invoices were found.",
                proposition="Invoices were found.", contradiction="No invoices were found.", threshold=0.5)),
            ("structured_unicode", "noul", dict(
                state={"status": "paid", "currency": "€", "note": "Café invoice"},
                instructions="The invoice has been paid.")),
            # Four explicit-opposite relations produce eight pairs. The NLI
            # backend truncates each long premise to its 512-position limit.
            ("capacity_relations", "relations", dict(items=[
                dict(state="An invoice was found. " + "The document record remains available. " * 300,
                     proposition=f"Invoice number {index + 1} was found.",
                     contradiction=f"Invoice number {index + 1} was not found.")
                for index in range(4)
            ], threshold=0.5)),
        ]
        captured = []
        try:
            for name, method, arguments in cases:
                batches.clear()
                print("BEGIN reference " + name, flush=True)
                result = getattr(engine, method)(**arguments)
                if not batches or any("logits" not in batch for batch in batches):
                    raise ValueError("Reference computation did not capture model execution")
                finite(result)
                finite(batches)
                if name == "chat_route" and result.get("choice") != "find":
                    raise ValueError("Existing Chat route fixture did not select find")
                captured.append(dict(name=name, method=method, arguments=arguments,
                                     result=result, batches=list(batches)))
                print("PASS reference " + name, flush=True)
        finally:
            pre.remove()
            post.remove()
        inventory(model, assets["files"])
        report = dict(format="samosa.inference.opendecision-reference.v1",
                      reference_revision=revision, model_assets=assets,
                      versions={name: importlib.metadata.version(name)
                                for name in ["torch", "transformers", "tokenizers", "sentencepiece"]},
                      weights_dtype=str(next(classifier.parameters()).dtype),
                      label2id=classifier.config.label2id,
                      max_position_embeddings=classifier.config.max_position_embeddings,
                      tokenizer_class=type(engine.classifier.tokenizer).__name__,
                      tokenizer_fast=engine.classifier.tokenizer.is_fast,
                      tokenizer=dict(file=tokenizer_output.name, bytes=len(tokenizer_bytes),
                                     sha256=hashlib.sha256(tokenizer_bytes).hexdigest()), cases=captured)
        descriptor = os.open(tokenizer_output, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(descriptor, "wb") as destination:
            destination.write(tokenizer_bytes)
            destination.flush()
            os.fsync(destination.fileno())
        descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
        with os.fdopen(descriptor, "w") as destination:
            json.dump(report, destination, ensure_ascii=False, allow_nan=False, indent=2)
            destination.write("\n")
            destination.flush()
            os.fsync(destination.fileno())
        print(f"PASS pinned OpenDecision reference: {len(captured)} cases; {output}", flush=True)


if __name__ == "__main__":
    main()
