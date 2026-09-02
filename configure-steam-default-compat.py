#!/data/data/com.termux/files/usr/bin/python3
"""Select the Termux Box64 compatibility tool once, without overriding custom choices."""

import argparse
import re
from pathlib import Path

BOX64 = "proton_bionic_termux"
TERMUX_TOOLS = {BOX64, "proton_bionic_fex_termux"}
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|[{}]')


def value(token: str) -> str:
    return token[1:-1] if token.startswith('"') else token


def block_end(tokens, opening):
    depth = 0
    for index in range(opening, len(tokens)):
        if tokens[index].group() == "{":
            depth += 1
        elif tokens[index].group() == "}":
            depth -= 1
            if depth == 0:
                return index
    raise ValueError("unterminated VDF block")


def named_block(tokens, name):
    for index, token in enumerate(tokens[:-1]):
        if value(token.group()) == name and tokens[index + 1].group() == "{":
            return index + 1, block_end(tokens, index + 1)
    return None


def direct_child_block(tokens, opening, closing, name):
    depth = 1
    index = opening + 1
    while index < closing:
        current = tokens[index].group()
        if current == "{":
            depth += 1
        elif current == "}":
            depth -= 1
        elif depth == 1 and value(current) == name:
            if index + 1 < closing and tokens[index + 1].group() == "{":
                return index + 1, block_end(tokens, index + 1)
        index += 1
    return None


def closing_indent(text, position):
    line = text.rfind("\n", 0, position) + 1
    return text[line:position]


def mapping_entry(indent):
    child = indent + "\t"
    field = child + "\t"
    return (
        f'{child}"0"\n{child}{{\n'
        f'{field}"name"\t\t"{BOX64}"\n'
        f'{field}"config"\t\t""\n'
        f'{field}"priority"\t\t"250"\n'
        f'{child}}}\n'
    )


def configure(text):
    tokens = list(TOKEN.finditer(text))
    mapping = named_block(tokens, "CompatToolMapping")
    if mapping:
        opening, closing = mapping
        default = direct_child_block(tokens, opening, closing, "0")
        if default:
            default_open, default_close = default
            for index in range(default_open + 1, default_close - 1):
                if (value(tokens[index].group()) == "name" and
                        tokens[index + 1].group().startswith('"')):
                    current = value(tokens[index + 1].group())
                    if current in TERMUX_TOOLS or not current.startswith("proton_"):
                        return text, current
                    start, end = tokens[index + 1].span()
                    return text[:start] + f'"{BOX64}"' + text[end:], BOX64
            raise ValueError("default compatibility mapping has no name")
        position = tokens[closing].start()
        indent = closing_indent(text, position)
        return text[:position] + mapping_entry(indent) + text[position:], BOX64

    steam = named_block(tokens, "Steam")
    if not steam:
        raise ValueError("Steam VDF block was not found")
    _, closing = steam
    position = tokens[closing].start()
    indent = closing_indent(text, position)
    child = indent + "\t"
    addition = (
        f'{child}"CompatToolMapping"\n{child}{{\n'
        f'{mapping_entry(child)}{child}}}\n'
    )
    return text[:position] + addition + text[position:], BOX64


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--marker", required=True, type=Path)
    args = parser.parse_args()
    if args.marker.exists():
        return

    args.config.parent.mkdir(parents=True, exist_ok=True)
    if args.config.exists():
        updated, selected = configure(args.config.read_text(encoding="utf-8"))
    else:
        updated = (
            '"InstallConfigStore"\n{\n\t"Software"\n\t{\n\t\t"Valve"\n'
            '\t\t{\n\t\t\t"Steam"\n\t\t\t{\n'
            '\t\t\t\t"CompatToolMapping"\n\t\t\t\t{\n'
            + mapping_entry("\t\t\t\t") +
            '\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n'
        )
        selected = BOX64
    temporary = args.config.with_suffix(args.config.suffix + ".tmp")
    temporary.write_text(updated, encoding="utf-8")
    temporary.replace(args.config)
    args.marker.write_text(selected + "\n", encoding="utf-8")
    print(f"Steam default compatibility tool: {selected}")


if __name__ == "__main__":
    main()
