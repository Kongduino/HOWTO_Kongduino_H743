//#define PRREG(x) SerialUSB.print(#x" 0x"); SerialUSB.println(x,HEX)
bool aes_hw_inited = 0;

void delay_ms(uint16_t ms) {
  mp_uint_t target = mp_hal_ticks_ms() + ms;
  while(mp_hal_ticks_ms() < target) ;
}

void trng_init(uint8_t* buffer, uint8_t len) {
  MCLK->APBCMASK.bit.TRNG_ = 1; // Enable Main Clock (MCLK) for TRNG
  TRNG->CTRLA.bit.ENABLE = 1; // Enable TRNG
  TRNG->INTENSET.bit.DATARDY = 1; // Enable TRNG interrupt when data ready
  delay_ms(10); // Short delay to allow generation of new random number
  // len must be a multiple of 4!
  uint32_t r;
  uint8_t ix,  ln = len >> 2;
  ln = ln << 2;
  for (ix = 0; ix < ln; ix += 2) {
    while ((TRNG->INTFLAG.reg & TRNG_INTFLAG_DATARDY) == 0) ; // 84 APB cycles
    r = TRNG->DATA.reg;
    buffer[ix] = (r >> 24) & 0xFF;
    buffer[ix + 1] = (r >> 16) & 0xFF;
    // buffer[ix + 2] = (r >> 8) & 0xFF;
    // buffer[ix] = r & 0xFF;
  }
  MCLK->APBCMASK.bit.TRNG_ = 0; // Disable Main Clock (MCLK) for TRNG
  TRNG->CTRLA.bit.ENABLE = 0; // Disable TRNG
  TRNG->INTENSET.bit.DATARDY = 0; // Disable TRNG interrupt when data ready
  delay_ms(10);
}

void aes_init() {
  // PRREG(MCLK->APBCMASK.reg); //aes bit 9
  // PRREG(REG_PAC_STATUSC); // aes bit 9
  MCLK->APBCMASK.reg |= MCLK_APBCMASK_AES;
  // PRREG(MCLK->APBCMASK.reg);
  // PRREG(REG_AES_CTRLA);
  // PRREG(REG_AES_CTRLB);
}

void aes_setkey(const uint8_t *key, size_t keySize) {
  memcpy((uint8_t *)&REG_AES_KEYWORD0, key, keySize);
}

void aes_cbc_hw_encrypt(const uint8_t *plaintext, uint8_t *ciphertext, size_t size, uint8_t *keyAes128, const uint8_t iv[16]) {
  if (aes_hw_inited==0) aes_init();
  aes_setkey(keyAes128, 16);
  int i;
  memcpy((uint8_t *)&REG_AES_INTVECTV0, iv, 16);
  REG_AES_CTRLA = 0;
  REG_AES_CTRLA = AES_CTRLA_AESMODE_CBC | AES_CTRLA_CIPHER_ENC | AES_CTRLA_ENABLE;
  REG_AES_CTRLB |= AES_CTRLB_NEWMSG;
  uint32_t *wp = (uint32_t *) plaintext; // need to do by word ?
  uint32_t *wc = (uint32_t *) ciphertext;
  // block 4-words  16B
  int word = 0;
  while (size > 0) {
    for (i = 0;  i < 4; i++) REG_AES_INDATA = wp[i + word];
    REG_AES_CTRLB |=  AES_CTRLB_START;
    while ((REG_AES_INTFLAG & AES_INTENCLR_ENCCMP) == 0);  // wait for done
    for (i = 0;  i < 4; i++) wc[i + word] = REG_AES_INDATA;
    size -= 16;
    word += 4;
  }
}

void aes_cbc_hw_decrypt(const uint8_t *ciphertext, uint8_t *plaintext, size_t size, uint8_t *keyAes128, const uint8_t iv[16]) {
  if (aes_hw_inited==0) aes_init();
  int i;
  memcpy((uint8_t *)&REG_AES_INTVECTV0, iv, 16);
  REG_AES_CTRLA = 0;
  REG_AES_CTRLA = AES_CTRLA_AESMODE_CBC | AES_CTRLA_CIPHER_DEC | AES_CTRLA_ENABLE;
  REG_AES_CTRLB |= AES_CTRLB_NEWMSG ;
  uint32_t *wp = (uint32_t *) plaintext; // need to do by word ?
  uint32_t *wc = (uint32_t *) ciphertext;
  // block 4-words  16B
  int word = 0;
  while (size > 0) {
    for (i = 0;  i < 4; i++) REG_AES_INDATA = wc[i + word];
    REG_AES_CTRLB |= AES_CTRLB_START;
    while ((REG_AES_INTFLAG & AES_INTENCLR_ENCCMP) == 0);  // wait for done
    for (i = 0;  i < 4; i++) wp[i + word] = REG_AES_INDATA;
    size -= 16;
    word += 4;
  }
}

void sha_hw(const uint8_t *blocks, uint8_t *myHash) {
  static uint8_t hash[128] __attribute__((aligned(128)));
  static IcmDescriptor dscr[4] __attribute__((aligned(64))); // RADDR RCFG RCTRL RNEXT
  MCLK->APBCMASK.reg |= MCLK_APBCMASK_ICM;
  dscr[0].RADDR.reg = (uint32_t)blocks;
  dscr[0].RCFG.reg = ICM_RCFG_ALGO(1) | ICM_RCFG_EOM_YES; // RSA256
  dscr[0].RCTRL.reg = sizeof(blocks) / 64; // number of blocks
  dscr[0].RNEXT.reg = 0;
  REG_ICM_DSCR = (uint32_t)&dscr;
  REG_ICM_HASH = (uint32_t)hash;
  REG_ICM_CFG = 0; // use RCFG
  REG_ICM_CTRL = ICM_CTRL_ENABLE;
  while ((REG_ICM_ISR & ICM_ISR_RHC(1)) == 0); // wait until done
  for(uint16_t i = 0; i < 32; i++) myHash[i] = hash[i];
  // memcpy(myHash, hash, 32);
}
