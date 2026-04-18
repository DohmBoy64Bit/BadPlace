print("BadPlace | Starting File System API Verification Suite...")

print("Test 1: writefile()")
writefile("test_data.txt", "Hello from BadPlace FileSystem Sandbox!")
print("Success: test_data.txt written.")

print("Test 2: makefolder() & isfolder()")
makefolder("SubFolderTest")
if isfolder("SubFolderTest") then
    print("Success: Folder SubFolderTest created and verified.")
else
    print("Failed: isfolder returned false for SubFolderTest")
end

print("Test 3: isfile() & readfile()")
if isfile("test_data.txt") then
    local data = readfile("test_data.txt")
    print("readfile content: " .. tostring(data))
    if data == "Hello from BadPlace FileSystem Sandbox!" then
        print("Success: readfile verified.")
    else
        print("Failed: readfile returned corrupted generic data")
    end
else
    print("Failed: isfile returned false for test_data.txt")
end

print("Test 4: listfiles()")
local files = listfiles("")
if type(files) == "table" then
    print("Success: listfiles() returned a table with " .. #files .. " items.")
    for i, v in ipairs(files) do
        print(" - " .. tostring(i) .. ": " .. tostring(v))
    end
else
    print("Failed: listfiles() did not return a table")
end

print("Test 5: Sandbox Boundary Enforcement (Security)")
print("Attempting to writefile to ../../../windows_system32_test.txt")
writefile("../../../windows_system32_test.txt", "MALWARE")
if isfile("../../../windows_system32_test.txt") then
    print("CRITICAL FAILURE: Jailbreak successful! Path normalizer failed!")
else
    print("Success: Sandbox rejected out-of-bounds path!")
end

print("BadPlace | File System API Suite Complete \xE2\x9C\x85")
