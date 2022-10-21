[
    path(..
         | (.power_rail?,
            .pin_status?,
            .interface_status?,
            .protocol_status?,
            .firmware_status?,
            .data_dump?)
         | values
         | .[]) as $path
    | {$path} + getpath($path)
]
