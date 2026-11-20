#include "bootutil/sign_key.h"
#include "verify_key.h"

_Static_assert(sizeof(ecdsa_pub_key) == 91, "Expected a DER-encoded ECDSA P-256 public key");

const struct bootutil_key bootutil_keys[] = {
    { .key = ecdsa_pub_key, .len = &ecdsa_pub_key_len },
};

const int bootutil_key_cnt = 1;
