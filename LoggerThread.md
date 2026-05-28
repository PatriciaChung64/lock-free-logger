The purpose of the logger thread:
- Continuously dequeue from their own queue
- Reconstruct log entries into data rows
- Batch 10 data rows then write to disk (file)

What does it need?
Very similar to worker thread, just that the lambda needs to be amended.

Does not need threadID like worker thread, since we don't actually care which logger thread wrote on the file.

