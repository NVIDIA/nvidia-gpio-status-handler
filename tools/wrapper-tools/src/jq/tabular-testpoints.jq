# Input: array of testpoints, supplied with "path": <array> property
[.[]
 | {
     "device": .path | .[0],
     "layer": .path | .[1],
     "index": .path | .[2],
     "name" : .name,
     "expected_value": .expected_value,
     ".accessor.type": .accessor.type?,
     ".accessor.executable": .accessor.executable?,
     ".accessor.arguments": .accessor.arguments?,
     ".accessor.device_name": .accessor.device_name?,
     ".accessor.interface": .accessor.interface?,
     ".accessor.object": .accessor.object?,
     ".accessor.property": .accessor.property?
 }
] | unique
