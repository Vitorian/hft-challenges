// String Map trait — your solution must implement this.
// Do NOT modify this file — it will be overwritten during certified runs.

pub trait StringMap {
    fn new() -> Self;

    /// Insert a key-value pair. Keys are unique (no duplicates).
    /// key_len <= 16.
    fn insert(&mut self, key: &[u8], value: u32);

    /// Look up a key. Returns Some(value) if found, None otherwise.
    fn find(&self, key: &[u8]) -> Option<u32>;
}
