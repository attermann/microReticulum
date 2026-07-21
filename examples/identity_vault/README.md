# simple encryption test

This allows encryption-at-rest for sent-messages and identity.

I only have a CardputerADV for testing, so I do it like this:

```sh
# compile and upload
pio run -t upload -e cardputeradv

# connect to serial
pio device monitor -e cardputeradv
```

Basic idea is you create a password-protected identity on SD card, then on reboot it will ask you for that password and still work with same identity. Various tests are performed to validate that idenity is working. The initial password-check is pretty slow to help defend against offline brute-force attacks, but it's hash-verified, and after that your identity is used (from memory) for full-speed operation, as it normally would be.

I also included [identity_tool](./identity_tool.py) for offline testing with the files.

The password for test files is `1234`.