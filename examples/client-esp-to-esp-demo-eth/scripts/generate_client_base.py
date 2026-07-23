from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
base_source = project_dir.parent / "common" / "client-demo" / "src" / "main.cpp"
generated_source = project_dir / "src" / "generated_client_base.inc"

source = base_source.read_text(encoding="utf-8")
source = source.replace("\nvoid setup() {", "\nvoid espToEspBaseSetup() {", 1)
source = source.replace("\nvoid loop() {", "\nvoid espToEspBaseLoop() {", 1)
source += "\n#include \"BacnetDemoFormat.cpp\"\n"
source += "#include \"BacnetDemoLogging.cpp\"\n"
source += "#include \"BacnetDemoPropertyBrowser.cpp\"\n"
source += "#include \"BacnetDemoWatchedAnalogValue.cpp\"\n"

if not generated_source.exists() or generated_source.read_text(encoding="utf-8") != source:
    generated_source.write_text(source, encoding="utf-8")
