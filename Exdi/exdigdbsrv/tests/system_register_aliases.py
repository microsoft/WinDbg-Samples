"""Compile and exercise the production register-selection code without a GDB target."""

import argparse
import json
import pathlib
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def function_body(source, signature):
    start = source.index("{", source.index(signature))
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start + 1:end - 1]


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    library = root.joinpath("GdbSrvControllerLib")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=pathlib.Path,
                        default=library.joinpath("XmlDataHelpers.cpp"))
    parser.add_argument("--compiler", nargs="+", default=["cl"])
    args = parser.parse_args()

    source = args.source.read_text(encoding="latin-1")
    handler = function_body(
        source, "bool XmlDataSystemRegister::HandleMapSystemRegAccessCode")
    selection = handler[handler.index("            for (auto it = itVector"):]
    selection = selection[:selection.index("            isDone = true;")]
    present = function_body(
        source, "inline bool XmlDataSystemRegister::IsRegisterPresent")

    registers = ET.parse(library.joinpath("systemregisters.xml")).getroot()
    entries = []
    for entry in registers.find("./SystemRegisters[@Architecture='ARM64']"):
        op0, op1, crn, crm, op2 = map(int, entry.attrib["AccessCode"].split(","))
        code = (op0 << 16) | (op1 << 12) | (crn << 8) | (crm << 4) | op2
        entries.append((code, entry.attrib["Name"]))
    for code, legacy in ((0x30100, "SCTLR"), (0x30c00, "VBAR")):
        assert [name for value, name in entries if value == code] == [
            legacy, legacy + "_EL1"]
    xml_entries = ",\n".join(
        f"{{{code}, {json.dumps(name)}}}" for code, name in entries)

    harness = r"""
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
using namespace std;
using AddressType = unsigned long long;
using SystemPairRegOrderNameType = pair<string, string>;
using SystemRegistersMapType = map<AddressType, SystemPairRegOrderNameType>;
struct RegistersStruct { string name; string nameOrder; };
struct Fixture {
    map<int, unique_ptr<vector<RegistersStruct>>> registers;
    map<int, unique_ptr<SystemRegistersMapType>> mappings;
    Fixture(initializer_list<RegistersStruct> exported) {
        registers.emplace(0, make_unique<vector<RegistersStruct>>(exported));
        mappings.emplace(0, make_unique<SystemRegistersMapType>());
    }
    static bool IsRegisterPresent(const string& regOrder,
                                 unique_ptr<SystemRegistersMapType>& spSysRegisterMap) {
        @PRESENT@
    }
    void Add(AddressType encodedValue, const char* nameBuf) {
        auto itVector = registers.find(0);
        auto itMap = mappings.find(0);
        SystemPairRegOrderNameType regNameOrder = {};
        regNameOrder.second = string(nameBuf);
        @SELECTION@
    }
    const SystemPairRegOrderNameType& Get(AddressType code) {
        return mappings.at(0)->at(code);
    }
};
const vector<pair<AddressType, const char*>> xmlEntries = {
    @XML@
};
void Require(bool condition, const char* message) {
    if (!condition) {
        cerr << message << '\n';
        exit(1);
    }
}
void Check(const char* legacy, AddressType code) {
    const string modern = string(legacy) + "_EL1";
    const vector<pair<vector<RegistersStruct>, string>> cases = {
        {{{legacy, "a"}}, legacy},
        {{{modern, "b"}}, modern},
        {{{legacy, "a"}, {modern, "b"}}, legacy},
        {{}, legacy}
    };
    for (const auto& scenario : cases) {
        Fixture fixture({});
        *fixture.registers.at(0) = scenario.first;
        fixture.registers.at(0)->push_back({"ELR_EL1", "77"});
        for (const auto& entry : xmlEntries) {
            fixture.Add(entry.first, entry.second);
        }
        const auto& selected = fixture.Get(code);
        Require(selected.second == scenario.second, "Incorrect alias selected");
        const string expected = scenario.first.empty() ? "n/a" :
            scenario.first.front().nameOrder;
        Require(selected.first == expected, "Incorrect GDB register order");
        Require(fixture.Get(0x30401) == make_pair(string("77"), string("ELR_EL1")),
                "Unrelated register mapping changed");
    }
    Fixture reversed({{legacy, "a"}, {modern, "b"}});
    reversed.Add(code, modern.c_str());
    reversed.Add(code, legacy);
    reversed.Add(code, modern.c_str());
    Require(reversed.Get(code) == make_pair(string("b"), modern),
            "First usable alias was not preserved");
    Fixture multiple({{modern, "b"}});
    multiple.Add(code, "UNAVAILABLE");
    multiple.Add(code, legacy);
    multiple.Add(code, modern.c_str());
    multiple.Add(code, "ALSO_UNAVAILABLE");
    Require(multiple.Get(code) == make_pair(string("b"), modern),
            "Fallback across multiple unavailable aliases failed");
    Fixture reserved({{modern, "b"}});
    reserved.Add(0x123, modern.c_str());
    reserved.Add(code, legacy);
    reserved.Add(code, modern.c_str());
    Require(reserved.Get(code).first == "n/a",
            "An already-mapped GDB register order was reused");
}
int main() {
    Check("VBAR", 0x30c00);
    Check("SCTLR", 0x30100);
    cout << "14 alias-selection cases passed\n";
}
"""
    harness = (harness.replace("@PRESENT@", present)
               .replace("@SELECTION@", selection)
               .replace("@XML@", xml_entries))
    with tempfile.TemporaryDirectory(prefix="exdi-alias-tests-") as temporary:
        directory = pathlib.Path(temporary)
        cpp = directory.joinpath("aliases.cpp")
        executable = directory.joinpath("aliases.exe")
        cpp.write_text(harness, encoding="utf-8")
        if pathlib.Path(args.compiler[0]).stem.lower() == "cl":
            flags = ["/nologo", "/std:c++17", "/EHsc", "/W4", "/WX", "/MT",
                     str(cpp), f"/Fe:{executable}", f"/Fo:{directory}\\aliases.obj"]
        else:
            flags = ["-std=c++17", "-Wall", "-Wextra", str(cpp),
                     "-o", str(executable)]
        subprocess.run(args.compiler + flags, check=True, cwd=root)
        subprocess.run([str(executable)], check=True, cwd=root)


if __name__ == "__main__":
    main()
