find_package(unofficial-libmagic REQUIRED)
find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)

# Creates C resources file from a binary file
function(create_resources file output)
    message(STATUS "Copy content of ${file} as C-array in ${output}")
    # Create empty output file
    file(WRITE ${output} "")
    # Collect input files
        # Get short filename
        string(REGEX MATCH "([^/]+)$" filename ${file})
        # Replace filename spaces & extension separator for C compatibility
        string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
        # Read hex data from file
        file(READ ${file} filedata HEX)
        # Convert hex data for C compatibility
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
        # Append data to output file
        file(APPEND ${output} "const unsigned char ${filename}[] = {${filedata}};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
endfunction()

create_resources("${unofficial-libmagic_DICTIONARY}" src/include/magic_mgc.hpp)
