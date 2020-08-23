/*
   Trentanove - PREVIEW version - Not use for real Bitcoin wallets
   https://pagami.it/trentavove

   Based on:
   - Cryptographic primitives by Libwally
   - VGA/Keyboard by FabGL Fabrizio Di Vittorio

   Copyright (c) 2020 Valerio Vaccaro. All rights reserved.

   NOTE:
   - need accurate review and tests
   - mnemonic is printed on serial for debug
   - mouse/phrase entropy are still missing
*/

#include <esp32-hal-log.h>
#include <fabgl.h>
#include <wally_core.h>
#include <wally_crypto.h>
#include <wally_bip39.h>
#include <wally_bip32.h>
#include <wally_address.h>

#define WORDS_NUMBER 2048 // Number of words for BIP39

fabgl::VGAController VGAController;
fabgl::PS2Controller PS2Controller;
fabgl::Terminal      Terminal;

typedef enum {
  ENTROPY_SOURCE_DICE,
  ENTROPY_SOURCE_CARDS,
  ENTROPY_SOURCE_PHRASE,
  ENTROPY_SOURCE_MOUSE,
  ENTROPY_SOURCE_RANDOM,
} entropy_source_t;

entropy_source_t entropy_source = ENTROPY_SOURCE_DICE;

void print_header() {
  Terminal.write("\e[40;32m"); // background: black, foreground: green
  Terminal.write("\e[2J");     // clear screen
  Terminal.write("\e[1;1H");   // move cursor to 1,1
  slow_printf("   _                  _\r\n");
  slow_printf("  | |                | |\r\n");
  slow_printf("  | |_ _ __ ___ _ __ | |_ __ _ _ __   _____   _____\r\n");
  slow_printf("  | __| '__/ _ \\ '_ \\| __/ _` | '_ \\ / _ \\ \\ / / _ \\\r\n");
  slow_printf("  | |_| | |  __/ | | | || (_| | | | | (_) \\ V /  __/\r\n");
  slow_printf("   \\__|_|  \\___|_| |_|\\__\\__,_|_| |_|\\___/ \\_/ \\___|  PREVIEW\r\n");
  slow_printf("\r\n");
  slow_printf("         2020 by Valerio Vaccaro  -  https://pagami.io/trentanove\r\n");
  slow_printf("================================================================================\r\n");
}

void slow_printf(const char * format, ...) {
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    for (int i = 0; i < size; ++i) {
      Terminal.write(buf[i]);
      delay(1);
    }
  }
  va_end(ap);
}

// read an integer from serial (from 1 to maximum_value)
int read_int(int maximum_value) {
  while (true) {
    int new_integer = Serial.parseInt();
    if ((new_integer > 0) && (new_integer < maximum_value + 1)) {
      return (new_integer - 1);
    }
  }
}

int read_int_kb() {
  auto keyboard = PS2Controller.keyboard();
  keyboard->setLayout(&fabgl::USLayout);
  String buf = "";
  while (true) {
    if (keyboard->virtualKeyAvailable()) {
      bool down;
      auto vk = keyboard->getNextVirtualKey(&down);
      if (down) {
        char c = keyboard->virtualKeyToASCII(vk);
        if (c > 47 && c < 58) {
          buf = buf + c;
          slow_printf("%c", c);
        } else if (c == 127) {  // delete
          buf.substring(0, buf.length() - 1);
        } else if (c == 13) {  // carriage return
          break;
        }
      }
    }
  }
  return buf.toInt();
}

// create an uniform distributed random number (from 0 to words_number) using external random source (from 1 to sides)
int uniform_generator(int words_number, int sides) {
  int r = ceil(log10(words_number) / log10(sides));
  int candidate = 0;
  for (int power = 0; power < r - 1; power++) {
    int new_value = 0;
    while (true) {
      slow_printf("\r\nInput a new value (1-%d):", sides);
      new_value  =  read_int_kb();
      if ((new_value > 0) && (new_value < sides + 1))
        break;
      slow_printf(" Invalid value, retry.");
    }
    new_value = new_value - 1;
    int res = candidate + pow(sides, power) * new_value;
    slow_printf(" - buffer %d + (%d ^ %d) * %d = %d", candidate, sides, power, new_value, res);
    Serial.print(" accumulator "); Serial.println(res);
    candidate = res;
  }
  // last can be repeated until valid
  slow_printf("\r\nLast value!");
  while (true) {
    int new_value = 0;
    while (true) {
      slow_printf("\r\nInput a new value (1-%d):", sides);
      new_value  =  read_int_kb();
      if ((new_value > 0) && (new_value < sides + 1))
        break;
    }
    new_value = new_value - 1;
    int res = candidate + pow(sides, r - 1) * new_value;
    slow_printf(" - buffer %d + (%d ^ %d) * %d = %d", candidate, sides, r - 1, new_value, res);
    if (res < WORDS_NUMBER)
      return res;
    slow_printf("\r\nPlease retry!");
  }
}

// extract nth word from a string using a special char as separator
String get_n_word(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// print formatted mnemonic
void print_mnemonic(size_t seed_length, String finalized_mnemonic, size_t columns_num) {
  for (int x = 0; x < seed_length; x++) {
    String wordn = get_n_word(finalized_mnemonic, ' ', x);
    size_t col;
    col = (x % columns_num) * (Terminal.getColumns() / columns_num);
    size_t row;
    row =  10 + (x / columns_num);
    Terminal.write( (String("\e[") + row + String(";") + col + String("H")).c_str() ); // move cursor
    slow_printf(" %2d %s", x + 1, wordn.c_str());
  }
}

// print cards encoding table
void print_deck() {
  slow_printf("= Clubs ========================================================================\r\n");
  slow_printf("  1:A   2:2   3:3   4:4   5:5   6:6   7:7   8:8   9:9  10:10  11:J  12:Q  13:K\r\n");
  slow_printf("= Diamonds =====================================================================\r\n");
  slow_printf(" 14:A  15:2  16:3  17:4  18:5  19:6  20:7  21:8  22:9  23:10  24:J  25:Q  26:K\r\n");
  slow_printf("= Hearts =======================================================================\r\n");
  slow_printf(" 27:A  28:2  29:3  30:4  31:5  32:6  33:7  34:8  35:9  36:10  37:J  38:Q  39:K\r\n");
  slow_printf("= Spades =======================================================================\r\n");
  slow_printf(" 40:A  41:2  42:3  43:4  44:5  45:6  46:7  47:8  48:9  49:10  50:J  51:Q  52:K\r\n");
  slow_printf("================================================================================\r\n");
}

// generate mnemonic
void generate_mnemonic() {
  auto keyboard = PS2Controller.keyboard();
  print_header();

  // get seed lenght
  int seed_length = 0;
  while (true) {
    slow_printf("\r\nInsert seed lenght (12-15-18-21-24): ");
    seed_length =  read_int_kb();
    if ((seed_length == 12) || (seed_length == 15) || (seed_length == 18) || (seed_length == 21) || (seed_length == 24)) {
      break;
    }
    slow_printf("\r\nPlease retry!");
  }

  // get entropy method
  while (true) {
    int entropy_source_num = 0;
    while (true) {
      slow_printf("\r\nHow you will generate randomness (1-dice, 2-cards, 3-hash phrase TBD, \r\n 4-mouse TBD, 5-ESP32 random ONLY FOR TEST!): ");
      entropy_source_num = read_int_kb();
      if ((entropy_source_num > 0) && (entropy_source_num < 5 + 1))
        break;
      slow_printf("\r\nPlease retry!");
    }
    switch (entropy_source_num) {
      case 1:
        entropy_source = ENTROPY_SOURCE_DICE;
        break;
      case 2:
        entropy_source = ENTROPY_SOURCE_CARDS;
        break;
      case 3:
        entropy_source = ENTROPY_SOURCE_PHRASE;
        break;
      case 4:
        entropy_source = ENTROPY_SOURCE_MOUSE;
        break;
      case 5:
        entropy_source = ENTROPY_SOURCE_RANDOM;
        break;
    }
    if (entropy_source_num > 0)
      break;
    slow_printf("\r\nPlease retry!");
  }

  // get sides if needed
  int sides = 0;
  switch (entropy_source) {
    case ENTROPY_SOURCE_DICE:
      while (true) {
        slow_printf("\r\nInput sides of the dice (2-64): ");
        sides = read_int_kb();
        if ((sides > 1) && (sides < 65)) {
          break;
        }
        slow_printf("\r\nPlease retry!");
      }
      break;
    case ENTROPY_SOURCE_CARDS:
      sides = 52;
      break;
    case ENTROPY_SOURCE_PHRASE:
      break;
    case ENTROPY_SOURCE_MOUSE:
      break;
    case ENTROPY_SOURCE_RANDOM:
      break;
  }

  struct words * words_english;
  int res = bip39_get_wordlist(NULL, &words_english);

  // get words
  String mnemonic = "";
  String finalized_mnemonic = "";
  int check_bits, value_bits, partial_index, prefix;
  int word_index = 0;
  int entropy_size = 0;
  char *phrase = NULL;
  unsigned char entropy[BIP39_ENTROPY_LEN_256];
  switch (entropy_source) {
    case ENTROPY_SOURCE_DICE:
      break;
    case ENTROPY_SOURCE_CARDS:
      for (int x = 0; x < seed_length - 1; x++) {
        print_header();
        slow_printf("Partial mnemonic: %s", mnemonic.c_str());
        slow_printf("\r\n================================================================================\r\n");
        slow_printf("Word: %d", x + 1);
        slow_printf("\r\n================================================================================\r\n");
        if (entropy_source == ENTROPY_SOURCE_CARDS)
          print_deck();
        word_index = 0;
        word_index = uniform_generator(WORDS_NUMBER, sides);
        char * new_word;
        res = bip39_get_word(words_english, word_index, &new_word);
        if (x == 0)
          mnemonic = new_word;
        else
          mnemonic = mnemonic + " " + new_word;
        slow_printf("\r\n================================================================================\r\n");
        slow_printf("adding words %s (%d) to the mnemonic", new_word, word_index);
        slow_printf("\r\n================================================================================\r\n");
        delay(1000);
        res = wally_free_string(new_word);
      }
      print_header();
      slow_printf("Partial mnemonic: %s", mnemonic.c_str());
      slow_printf("Word: %d (last)", seed_length);
      slow_printf("\r\n================================================================================\r\n");
      if (entropy_source == ENTROPY_SOURCE_CARDS)
        print_deck();
      check_bits = round(((log10(WORDS_NUMBER) / log10(2)) * seed_length) / 32);
      value_bits = (log10(WORDS_NUMBER) / log10(2)) - check_bits;
      // get last bits
      partial_index = 0;
      partial_index = uniform_generator(pow(2, round(value_bits)), sides);
      prefix = partial_index * pow(2, check_bits);
      for (int x = 0; x < pow(2, check_bits); x++) {
        char * new_word;
        res = bip39_get_word(words_english, prefix + x, &new_word);
        finalized_mnemonic = mnemonic + " " + new_word;
        res = bip39_mnemonic_validate(words_english, finalized_mnemonic.c_str());
        if (res == WALLY_OK) {
          print_header();
          wally_free_string(new_word);
          print_mnemonic(seed_length, finalized_mnemonic, 4);
          slow_printf("\r\n================================================================================\r\n");
          break;
        }
      }
      break;
    case ENTROPY_SOURCE_PHRASE:
      if (seed_length == 12) {
        entropy_size = BIP39_ENTROPY_LEN_128;
      }
      else if (seed_length == 15) {
        entropy_size = BIP39_ENTROPY_LEN_160;
      }
      else if (seed_length == 18) {
        entropy_size = BIP39_ENTROPY_LEN_192;
      }
      else if (seed_length == 21) {
        entropy_size = BIP39_ENTROPY_LEN_224;
      }
      else if (seed_length == 24) {
        entropy_size = BIP39_ENTROPY_LEN_256;
      }
      res = wally_sha256((const unsigned char*)"pippo", 5, entropy, BIP39_ENTROPY_LEN_256);
      res = bip39_mnemonic_from_bytes(NULL, entropy, entropy_size, &phrase);
      finalized_mnemonic = String(phrase);
      wally_free_string(phrase);
      print_header();
      print_mnemonic(seed_length, finalized_mnemonic, 4);
      slow_printf("\r\n================================================================================\r\n");
      break;
    case ENTROPY_SOURCE_MOUSE:
      finalized_mnemonic = "";
      break;
    case ENTROPY_SOURCE_RANDOM:
      if (seed_length == 12) {
        entropy_size = BIP39_ENTROPY_LEN_128;
      }
      else if (seed_length == 15) {
        entropy_size = BIP39_ENTROPY_LEN_160;
      }
      else if (seed_length == 18) {
        entropy_size = BIP39_ENTROPY_LEN_192;
      }
      else if (seed_length == 21) {
        entropy_size = BIP39_ENTROPY_LEN_224;
      }
      else if (seed_length == 24) {
        entropy_size = BIP39_ENTROPY_LEN_256;
      }
      for (int j = 0; j < entropy_size; j++) {
        entropy[j] = esp_random() % 0xff;
      }
      res = bip39_mnemonic_from_bytes(NULL, entropy, entropy_size, &phrase);
      finalized_mnemonic = String(phrase);
      wally_free_string(phrase);
      print_header();
      print_mnemonic(seed_length, finalized_mnemonic, 4);
      slow_printf("\r\n================================================================================\r\n");
      break;
  }

  // converting recovery phrase to seed
  uint8_t seed[BIP39_SEED_LEN_512];
  size_t len;
  res = bip39_mnemonic_to_seed(finalized_mnemonic.c_str(), "", seed, sizeof(seed), &len);

  Serial.println(finalized_mnemonic);

  // print derivation if needed //0 no derivation, 1 testnet 2 mainnet
  int derivation_type = 0;
  while (true) {
    slow_printf("\r\nInput derivation type (0 = Nothing, 1 = Bitcoin mainnet, 2 = Bitcoin testnet): ");
    derivation_type = read_int_kb();
    if ((derivation_type > -1) && (derivation_type < 3)) {
      break;
    }
    slow_printf("\r\nPlease retry!");
  }

  ext_key * root = NULL;
  char * addr = NULL;
  ext_key * account = NULL;
  char *xprv = NULL;
  ext_key * first_recv = NULL;
  uint32_t recv_path[] = {0, 0};

  uint32_t mainnet_path[] = {
    BIP32_INITIAL_HARDENED_CHILD + 84, // 84h
    BIP32_INITIAL_HARDENED_CHILD + 1, // 1h
    BIP32_INITIAL_HARDENED_CHILD     // 0h
  };

  uint32_t testnet_path[] = {
    BIP32_INITIAL_HARDENED_CHILD + 84, // 84h
    BIP32_INITIAL_HARDENED_CHILD + 1, // 1h
    BIP32_INITIAL_HARDENED_CHILD     // 0h
  };

  switch (derivation_type) {
    case 0:
      break;
    case 1:
      // MAINNET root HD key
      res = bip32_key_from_seed_alloc(seed, sizeof(seed), BIP32_VER_MAIN_PRIVATE, 0, &root);
      res = bip32_key_to_base58(root, BIP32_FLAG_KEY_PRIVATE, &xprv);
      slow_printf("\r\n================================================================================\r\n");
      slow_printf("Bitcoin Mainnet !!!\r\nRoot Key: ");
      slow_printf(xprv);
      Serial.print("Root key: ");
      Serial.println(xprv);
      // derivations
      slow_printf("\r\nDerivation: m/44'/1'/0'/0/0\r\n");
      Serial.print("Derivation: m/44'/1'/0'/0/0");
      res = bip32_key_from_parent_path_alloc(root, mainnet_path, 3, BIP32_FLAG_KEY_PRIVATE, &account);
      // key for the first address
      // we only need public key here, no need in private key
      res = bip32_key_from_parent_path_alloc(account, recv_path, 2, BIP32_FLAG_KEY_PUBLIC, &first_recv);
      // we don't need account anymore - free it
      bip32_key_free(account);
      // native segwit address
      res = wally_bip32_key_to_addr_segwit(first_recv, "tb", 0, &addr);
      Serial.print("Segwit address: ");
      Serial.println(addr);
      slow_printf("Segwit address .......... %s\r\n", addr);
      wally_free_string(addr);
      // nested segwit address
      res = wally_bip32_key_to_address(first_recv, WALLY_ADDRESS_TYPE_P2SH_P2WPKH, WALLY_ADDRESS_VERSION_P2SH_TESTNET, &addr);
      Serial.print("Nested segwit address: ");
      Serial.println(addr);
      slow_printf("Nested segwit address ... %s\r\n", addr);
      wally_free_string(addr);
      // legacy address
      res = wally_bip32_key_to_address(first_recv, WALLY_ADDRESS_TYPE_P2PKH, WALLY_ADDRESS_VERSION_P2PKH_TESTNET, &addr);
      Serial.print("Legacy address: ");
      Serial.println(addr);
      slow_printf("Legacy address .......... %s\r\n", addr);
      wally_free_string(addr);
      // free first recv key
      bip32_key_free(first_recv);
      wally_free_string(xprv);
      slow_printf("================================================================================\r\n");
      break;
    case 2:
      // TESTNET root HD key
      res = bip32_key_from_seed_alloc(seed, sizeof(seed), BIP32_VER_TEST_PRIVATE, 0, &root);
      res = bip32_key_to_base58(root, BIP32_FLAG_KEY_PRIVATE, &xprv);
      slow_printf("\r\n================================================================================\r\n");
      slow_printf("Bitcoin Testnet\r\nRoot Key: ");
      slow_printf(xprv);
      Serial.print("Root key: ");
      Serial.println(xprv);
      // derivations
      res = bip32_key_from_parent_path_alloc(root, testnet_path, 3, BIP32_FLAG_KEY_PRIVATE, &account);
      // key for the first address
      res = bip32_key_from_parent_path_alloc(account, recv_path, 2, BIP32_FLAG_KEY_PUBLIC, &first_recv);
      // we don't need account anymore - free it
      bip32_key_free(account);
      slow_printf("\r\nDerivation: m/84'/1'/0'/0/0\r\n");
      Serial.print("Derivation: m/84'/1'/0'/0/0");
      // native segwit address
      res = wally_bip32_key_to_addr_segwit(first_recv, "tb", 0, &addr);
      Serial.print("Segwit address: ");
      Serial.println(addr);
      slow_printf("Segwit address .......... %s\r\n", addr);
      wally_free_string(addr);
      // nested segwit address
      res = wally_bip32_key_to_address(first_recv, WALLY_ADDRESS_TYPE_P2SH_P2WPKH, WALLY_ADDRESS_VERSION_P2SH_TESTNET, &addr);
      Serial.print("Nested segwit address: ");
      Serial.println(addr);
      slow_printf("Nested segwit address ... %s\r\n", addr);
      wally_free_string(addr);
      // legacy address
      res = wally_bip32_key_to_address(first_recv, WALLY_ADDRESS_TYPE_P2PKH, WALLY_ADDRESS_VERSION_P2PKH_TESTNET, &addr);
      Serial.print("Legacy address: ");
      Serial.println(addr);
      slow_printf("Legacy address .......... %s\r\n", addr);
      wally_free_string(addr);
      // free first recv key
      bip32_key_free(first_recv);
      wally_free_string(xprv);
      slow_printf("================================================================================\r\n");
      break;
  }


  /**/



}

void setup() {
  Serial.begin(115200); delay(500); Serial.write("\n\n\n");
  VGAController.begin();
  VGAController.setResolution(VGA_640x240_60Hz);
  Terminal.begin(&VGAController);
  Terminal.enableCursor(true);
  PS2Controller.begin(PS2Preset::KeyboardPort0);
}

void loop() {
  generate_mnemonic();
  while (true) {
    delay(1000);
  }
}
