---
name: Report a Bug
about: Report a bug in order to get it fixed.
title: ''
labels: bug
assignees: vaxerski

---

**Please describe the bug**
A clear and concise description of what the bug is.

**Steps to reproduce:**
Do a, use program b...

**Expected behavior**
A clear and concise description of what you expected to happen.

**Screenshots**
If applicable, add screenshots to help explain your problem.

**Anything else?**

**Log:**
Please attach a log. (paste it into pastebin and paste here the url) The log can be found in a temp file located in /tmp/hypr/hypr.log.

**Coredump:**
If Hypr crashed, please attach a coredump. (paste into pastebind and paste here the url)

How to?

Systemd instructions:
`coredumpctl`
find the last ocurrence of Hypr and note the PID.
`coredumpctl info <PID>`
will print the coredump.
`coredumpctl info <PID> --no-pager | xclip -sel clip`
will copy it to the clipboard.
