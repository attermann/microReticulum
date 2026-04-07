# simple encryption test

This allows encryption-at-rest for sent-messages and identity.

I only have a CardputerADV for testing, so I do it like this:

```sh
# compile and upload
pio run -t upload -e cardputeradv

# connect to serial
pio device monitor -e cardputeradv
```

Basic idea is you create a password-protected identity on SD card, then on reboot it will ask you for that password and still work with same identity. Various tests are performed to validate that idenity is working. The initial password request is pretty slow to help defind against offline brute-forcing, but it's verified, and after that your idenity is used (from memory) for full-speed operation.