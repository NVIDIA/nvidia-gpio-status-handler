# Input: array of testpoints, supplied with "path": <array> property
[
    .[]
    | select(.accessor.type == "CMDLINE")
    | {
        "device": .path | .[0],
        "layer": .path | .[1],
        "index": .path | .[2],
        "name" : .name,
        "expected_value": .expected_value,
        ".accessor.executable": .accessor.executable?,
        ".accessor.arguments": .accessor.arguments?,
    }
]
