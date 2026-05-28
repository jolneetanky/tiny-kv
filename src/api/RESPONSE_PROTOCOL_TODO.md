# Response Protocol TODO

Goal: add a separate always-populated `message` field to `protocol::Response`,
while keeping `data` optional. This should make deleted/missing GET responses
return `ok=false` with `data=nullopt`, so `FlushRespectsDeletes` and
`CompactionRespectsDeletes` pass.

## Target Response Shape

```cpp
struct Response
{
    bool ok = false;
    std::string message;
    std::optional<std::string> data;
};
```

Suggested DB behavior:

- Successful GET: `{true, "OK", value}`
- Missing/tombstoned GET: `{false, "Key does not exist", std::nullopt}`
- Successful PUT/DEL/flush/compact: `{true, "OK", std::nullopt}`
- Usage or command errors: `{false, "<error message>", std::nullopt}`

## Wire Format

Use keyed, length-prefixed response fields:

```text
status=<STATUS> message=<message_len>:<message> data=<data_len>:<data>\n
```

Examples:

```text
status=OK message=2:OK data=5:world\n
status=ERR message=18:Key does not exist data=0:\n
status=OK message=2:OK data=0:\n
```

Rules:

- `status` is `OK` or `ERR`.
- `message` is always present.
- `data` is always encoded, but `data=0:` means `Response::data = std::nullopt`.
- `data_len > 0` means read exactly that many bytes into `Response::data`.
- Lengths are byte counts.
- The trailing newline remains the response frame delimiter.

## Implementation TODOs

- Update `protocol::Response` in `api/message.h` to add `std::string message`.
- Update `encodeResponse()` in `api/codec.cpp` to emit the keyed length-prefixed format.
- Update `decodeResponseLine()` to parse:
  - `status=`
  - `message=<len>:<payload>`
  - `data=<len>:<payload>`
- Reject malformed responses with a useful parse error.
- Update `DbImpl` response construction so error messages go into `message`, not `data`.
- Update `CommandHandler` response construction so error messages go into `message`, not `data`.
- Update CLI response printing:
  - success with data: print `OK data`
  - success without data: print `OK`
  - error: print `ERR: <message>`
- Update or add codec tests for:
  - success with data
  - error without data
  - message containing spaces
  - data containing spaces

## Acceptance Checks

Run focused tests:

```sh
./build/tests/tinykv_tests --gtest_filter=DbStorageTest.FlushRespectsDeletes
./build/tests/tinykv_tests --gtest_filter=DbStorageTest.CompactionRespectsDeletes
```

Then run the full test binary.
