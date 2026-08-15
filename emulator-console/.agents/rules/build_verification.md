# Build Verification

- Verify all code changes using the `eim` manager:
  ```bash
  eim select v5.5.5 && eim run "idf.py build"
  ```
- For clean builds:
  ```bash
  eim select v5.5.5 && eim run "idf.py fullclean build"
  ```
