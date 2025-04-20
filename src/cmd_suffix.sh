# Read only variables that contain UUIDs to help ensure no conflict for extended attribute names.
CLI_ARGS_MD5_UUID="2BC44991_B9F7_4DA9_97D6_143A1C087A54"
SELF_FILE_MD5_UUID="FE8954C1_37B5_4784_B725_C3DF127C8BD7"
INPUT_FILE_MD5_UUID="8DE5FF00_C2B2_4631_BB73_1AEFA1954DA8"
if [ ! -f "$INPUT" ]
then
    exit -1;
fi

if [ ! -f "$OUTPUT" ]
then
    OUTPUTDIR=$(dirname "$OUTPUT")
    mkdir -p "$OUTPUTDIR"
    touch "$OUTPUT"
fi

if ! INPUT_FILE_MD5SUM=$(getfattr --only-values --name="user.self_md5sum_$SELF_FILE_MD5_UUID" "$INPUT" 2>/dev/null)
then
    INPUT_FILE_MD5SUM=$(md5sum "$INPUT" | cut -f1 -d' ')
    setfattr --name="user.self_md5sum_$SELF_FILE_MD5_UUID" --value "$INPUT_FILE_MD5SUM" "$INPUT"
fi
CLI_ARGS_MD5SUM=$(echo "$PRESET $ARGS" | md5sum | cut -f1 -d' ')

DEST_CLI_ARGS_MD5SUM=$(getfattr --only-values --name="user.cli_args_md5sum_$CLI_ARGS_MD5_UUID" "$OUTPUT")
DEST_INPUT_FILE_MD5SUM=$(getfattr --only-values --name="user.input_file_md5sum_$INPUT_FILE_MD5_UUID" "$OUTPUT")
if  [ " $CLI_ARGS_MD5SUM "   == " $DEST_CLI_ARGS_MD5SUM " ] &&
    [ " $INPUT_FILE_MD5SUM " == " $DEST_INPUT_FILE_MD5SUM " ]
then
    exit 0
fi

rm "$OUTPUT"

HandBrakeCLI --output "$OUTPUT" --input "$INPUT" --preset "$PRESET" $ARGS

if [ $? -eq 0 ]
then
    setfattr --name="user.cli_args_md5sum_$CLI_ARGS_MD5_UUID" --value "$CLI_ARGS_MD5SUM" "$OUTPUT"
    setfattr --name="user.input_file_md5sum_$INPUT_FILE_MD5_UUID" --value "$INPUT_FILE_MD5SUM" "$OUTPUT"
else
    exit -1
fi
