# Test report

## Revision 178caf

- Commit hash: `178caf1f12a8f7f56b6deb429d031417ea6a6154`
- Commit message: `[main] Remove indication of motion detection`

### Known issues

- Scroll wheel starts to lag at some speed threshold. Unmounting C20 and C21 may help.
- The mouse initiates connection if it was forgotten by the client.

### Further improvements to already implemented functionality

- Do not start undirected advertising unless explicitly asked by user for privacy reasons.
- Enforce L3 security level for already established connections.
- VDD regulator is not getting disabled on power off.
