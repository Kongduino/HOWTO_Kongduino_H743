#include <stdio.h>
#include "py/obj.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/objstr.h"
#include "py/objarray.h"
#ifdef MICROPY_HW_H743
#include "stm32h7xx_hal_def.h"
#include "stm32h7xx_hal.h"
#endif
#ifdef MICROPY_HW_412Rx
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_hal.h"
#endif
#include "mphalport.h"
#include "kongduino/aes.c"
#ifdef SAMD51_HARDWARE_AES
#include "kongduino/samd51_aes.c"
#endif
#include "kongduino/crc.c"
#include "kongduino/qrcodegen.c"

static mp_obj_t kongduino_genText2(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t c_obj) {
  uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
  mp_buffer_info_t bufinfoA;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  // int aLen = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  char *myText = bufinfoA.buf;
  mp_buffer_info_t bufinfoB;
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  // int bLen = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  uint8_t *qr0 = bufinfoB.buf;
  int maxVersion = MP_OBJ_SMALL_INT_VALUE(c_obj);

  bool ok = qrcodegen_encodeText(
    (const char *)myText, tempBuffer, bufinfoB.buf, qrcodegen_Ecc_LOW,
    1, maxVersion, qrcodegen_Mask_AUTO, true);
  if (!ok) return mp_obj_new_int(-1);
  int size = qrcodegen_getSize(qr0);
  return mp_obj_new_int(size);
}

static mp_obj_t kongduino_crc(mp_obj_t a_obj) {
  int len;
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(a_obj, &bufinfo, MP_BUFFER_READ);
  len = bufinfo.len / mp_binary_get_size('@', bufinfo.typecode, NULL);
  uint8_t *buf = bufinfo.buf;
  //crcInit();
  crc ret = crcFast(buf, len);
  return mp_obj_new_int(ret);
}

static mp_obj_t kongduino_reverse_array(mp_obj_t a_obj) {
  int ret, half, i;
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(a_obj, &bufinfo, MP_BUFFER_READ);
  ret = bufinfo.len / mp_binary_get_size('@', bufinfo.typecode, NULL);
  half = ret >> 1;
  uint8_t *MyArray = bufinfo.buf;
  for (i = 0; i < half; i++) {
    uint8_t tmp = MyArray[i];
    MyArray[i] = MyArray[ret - 1 - i];
    MyArray[ret - 1 - i] = tmp;
  }
  return mp_obj_new_bool(1);
}

static mp_obj_t kongduino_hexdump(mp_obj_t a_obj) {
  int len, i;
  mp_buffer_info_t bufinfo;
  mp_get_buffer_raise(a_obj, &bufinfo, MP_BUFFER_READ);
  len = bufinfo.len / mp_binary_get_size('@', bufinfo.typecode, NULL);
  uint8_t *buf = bufinfo.buf;
  char alphabet[17] = "0123456789abcdef";
  uint16_t index;
  printf("    +------------------------------------------------+ +----------------+\n");
  printf("    |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |\n");
  for (i = 0; i < len; i += 16) {
    if (i % 128 == 0) printf("    +------------------------------------------------+ +----------------+\n");
    char s[] = "|                                                | |                |\n";
    // pre-formated line. We will replace the spaces with text when appropriate.
    uint8_t ix = 1, iy = 52, j;
    for (j = 0; j < 16; j++) {
      if (i + j < len) {
        uint8_t c = buf[i + j];
        // fastest way to convert a byte to its 2-digit hex equivalent
        s[ix++] = alphabet[(c >> 4) & 0x0F];
        s[ix++] = alphabet[c & 0x0F];
        ix++;
        if (c > 31 && c < 128) s[iy++] = c;
        else s[iy++] = '.';  // display ASCII code 0x20-0x7F or a dot.
      }
    }
    index = i / 16;
    // display line number then the text
    printf("%3x.%s", index, s);
  }
  printf("    +------------------------------------------------+ +----------------+\n");
  return mp_obj_new_bool(1);
}

static mp_obj_t kongduino_decryptAES_ECB(mp_obj_t a_obj, mp_obj_t b_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB;
  mp_buffer_info_t bufinfoA, bufinfoB;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  struct AES_ctx ctx;
  AES_init_ctx(&ctx, pKey);
  uint8_t rounds = lenA >> 4, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    // void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
    AES_ECB_decrypt(&ctx, buf + steps);
    steps += 16;
    // decrypts in place, 16 bytes at a time
  }
  return mp_obj_new_int(lenA);
}

static mp_obj_t kongduino_encryptAES_ECB(mp_obj_t a_obj, mp_obj_t b_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB;
  mp_buffer_info_t bufinfoA, bufinfoB;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  struct AES_ctx ctx;
  AES_init_ctx(&ctx, pKey);
  uint8_t rounds = lenA >> 4, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    AES_ECB_encrypt(&ctx, buf + steps);
    steps += 16;
    // encrypts in place, 16 bytes at a time
  }
  return mp_obj_new_int(lenA);
}

static mp_obj_t kongduino_encryptAES_CBC(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t c_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB, lenC;
  mp_buffer_info_t bufinfoA, bufinfoB, bufinfoC;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  mp_get_buffer_raise(c_obj, &bufinfoC, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  lenC = bufinfoC.len / mp_binary_get_size('@', bufinfoC.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  uint8_t *Iv = bufinfoC.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  if (lenC != reqLen) return mp_obj_new_int(-3);
  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, pKey, Iv);
  AES_CBC_encrypt_buffer(&ctx, buf, lenA);
  return mp_obj_new_int(lenA);
}

static mp_obj_t kongduino_decryptAES_CBC(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t c_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB, lenC;
  mp_buffer_info_t bufinfoA, bufinfoB, bufinfoC;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  mp_get_buffer_raise(c_obj, &bufinfoC, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  lenC = bufinfoC.len / mp_binary_get_size('@', bufinfoC.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  uint8_t *Iv = bufinfoC.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  if (lenC != reqLen) return mp_obj_new_int(-3);
  struct AES_ctx ctx;
  AES_init_ctx_iv(&ctx, pKey, Iv);
  AES_CBC_decrypt_buffer(&ctx, buf, lenA);
  return mp_obj_new_int(lenA);
}

#ifdef SAMD51_HARDWARE_AES
static mp_obj_t kongduino_encryptAES_CBC_hw(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t c_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB, lenC;
  mp_buffer_info_t bufinfoA, bufinfoB, bufinfoC;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  mp_get_buffer_raise(c_obj, &bufinfoC, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  lenC = bufinfoC.len / mp_binary_get_size('@', bufinfoC.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  uint8_t *Iv = bufinfoC.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  if (lenC != reqLen) return mp_obj_new_int(-3);
  uint8_t buf0[lenA];
  aes_cbc_hw_encrypt(buf, buf0, lenA, pKey, Iv);
  memcpy(buf, buf0, lenA);
  return mp_obj_new_int(lenA);
}

static mp_obj_t kongduino_decryptAES_CBC_hw(mp_obj_t a_obj, mp_obj_t b_obj, mp_obj_t c_obj) {
  uint8_t reqLen = 16;
  int lenA, lenB, lenC;
  mp_buffer_info_t bufinfoA, bufinfoB, bufinfoC;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  mp_get_buffer_raise(c_obj, &bufinfoC, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  lenC = bufinfoC.len / mp_binary_get_size('@', bufinfoC.typecode, NULL);
  uint8_t *buf = bufinfoA.buf;
  uint8_t *pKey = bufinfoB.buf;
  uint8_t *Iv = bufinfoC.buf;
  if (lenA % reqLen != 0) return mp_obj_new_int(-1);
  if (lenB != reqLen) return mp_obj_new_int(-2);
  if (lenC != reqLen) return mp_obj_new_int(-3);
  uint8_t buf0[lenA];
  aes_cbc_hw_decrypt(buf, buf0, lenA, pKey, Iv);
  memcpy(buf, buf0, lenA);
  return mp_obj_new_int(lenA);
}

static mp_obj_t kongduino_hw_sha256(mp_obj_t a_obj, mp_obj_t b_obj) {
  uint16_t lenA, lenB;
  mp_buffer_info_t bufinfoA, bufinfoB;
  mp_get_buffer_raise(a_obj, &bufinfoA, MP_BUFFER_READ);
  mp_get_buffer_raise(b_obj, &bufinfoB, MP_BUFFER_READ);
  lenA = bufinfoA.len / mp_binary_get_size('@', bufinfoA.typecode, NULL);
  lenB = bufinfoB.len / mp_binary_get_size('@', bufinfoB.typecode, NULL);
  if (lenA % 64 != 0) return mp_obj_new_int(-1);
  if (lenB != 32) return mp_obj_new_int(-2);
  uint8_t *blocksBuf = bufinfoA.buf;
  uint8_t *hashBuf = bufinfoB.buf;
  sha_hw(blocksBuf, hashBuf);
  return mp_obj_new_bool(1);
}
#endif

static MP_DEFINE_CONST_FUN_OBJ_1(kongduino_reverse_array_obj, kongduino_reverse_array);
static MP_DEFINE_CONST_FUN_OBJ_3(kongduino_genText2_obj, kongduino_genText2);
static MP_DEFINE_CONST_FUN_OBJ_1(kongduino_crc_obj, kongduino_crc);
static MP_DEFINE_CONST_FUN_OBJ_1(kongduino_hexdump_obj, kongduino_hexdump);
static MP_DEFINE_CONST_FUN_OBJ_2(kongduino_encryptAES_ECB_obj, kongduino_encryptAES_ECB);
static MP_DEFINE_CONST_FUN_OBJ_2(kongduino_decryptAES_ECB_obj, kongduino_decryptAES_ECB);
static MP_DEFINE_CONST_FUN_OBJ_3(kongduino_encryptAES_CBC_obj, kongduino_encryptAES_CBC);
static MP_DEFINE_CONST_FUN_OBJ_3(kongduino_decryptAES_CBC_obj, kongduino_decryptAES_CBC);
#ifdef SAMD51_HARDWARE_AES
static MP_DEFINE_CONST_FUN_OBJ_3(kongduino_encryptAES_CBC_hw_obj, kongduino_encryptAES_CBC_hw);
static MP_DEFINE_CONST_FUN_OBJ_3(kongduino_decryptAES_CBC_hw_obj, kongduino_decryptAES_CBC_hw);
static MP_DEFINE_CONST_FUN_OBJ_1(kongduino_hw_trng_obj, kongduino_hw_trng);
static MP_DEFINE_CONST_FUN_OBJ_2(kongduino_hw_sha256_obj, kongduino_hw_sha256);
#endif

static const mp_rom_map_elem_t kongduino_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_kongduino) },
  { MP_ROM_QSTR(MP_QSTR_reverse_array), MP_ROM_PTR(&kongduino_reverse_array_obj) },
  { MP_ROM_QSTR(MP_QSTR_genText2), MP_ROM_PTR(&kongduino_genText2_obj) },
  { MP_ROM_QSTR(MP_QSTR_crc), MP_ROM_PTR(&kongduino_crc_obj) },
  { MP_ROM_QSTR(MP_QSTR_hexdump), MP_ROM_PTR(&kongduino_hexdump_obj) },
  { MP_ROM_QSTR(MP_QSTR_decryptAES_ECB), MP_ROM_PTR(&kongduino_decryptAES_ECB_obj) },
  { MP_ROM_QSTR(MP_QSTR_encryptAES_ECB), MP_ROM_PTR(&kongduino_encryptAES_ECB_obj) },
  { MP_ROM_QSTR(MP_QSTR_decryptAES_CBC), MP_ROM_PTR(&kongduino_decryptAES_CBC_obj) },
  { MP_ROM_QSTR(MP_QSTR_encryptAES_CBC), MP_ROM_PTR(&kongduino_encryptAES_CBC_obj) },
#ifdef SAMD51_HARDWARE_AES
  { MP_ROM_QSTR(MP_QSTR_decryptAES_CBC_hw), MP_ROM_PTR(&kongduino_decryptAES_CBC_hw_obj) },
  { MP_ROM_QSTR(MP_QSTR_encryptAES_CBC_hw), MP_ROM_PTR(&kongduino_encryptAES_CBC_hw_obj) },
  { MP_ROM_QSTR(MP_QSTR_hw_trng), MP_ROM_PTR(&kongduino_hw_trng_obj) },
  { MP_ROM_QSTR(MP_QSTR_hw_sha256), MP_ROM_PTR(&kongduino_hw_sha256_obj) },
#endif
};
static MP_DEFINE_CONST_DICT(kongduino_module_globals, kongduino_module_globals_table);

const mp_obj_module_t kongduino_user_cmodule = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t *)&kongduino_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_kongduino, kongduino_user_cmodule);
