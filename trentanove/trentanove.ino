/*
   Trentanove - PREVIEW version - Not use for real Bitcoin
   https://pagami.it/trentavove 
   
   Based on:
   - Cryptographic primitives by Libwally
   - VGA/Keyboard by FabGL Fabrizio Di Vittorio

   Copyright (c) 2020 Valerio Vaccaro. All rights reserved.

   NOTE:
   - Keyboard is still not functional (due to covid-19 I can not access to a PS/2 keyboard).
   - need accurate review and test
   - mnemonic is exposed on serial port (only for fast debug)
   
*/

#include <fabgl.h>
#include <wally_core.h>
#include <wally_bip39.h>

#define WORDS_NUMBER 2048 // Number of words for BIP39

fabgl::VGAController VGAController;
fabgl::PS2Controller PS2Controller;
fabgl::Terminal      Terminal;

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

// create an uniform distributed random number (from 0 to words_number) using external random source (from 1 to sides)
int uniform_generator(int words_number, int sides) {
  int r = ceil(log10(words_number) / log10(sides));
  int candidate = 0;
  for (int power = 0; power < r - 1; power++) {
    String message;
    message = String("Input a new value (1-") + sides + String("): ");
    Serial.println(message);
    slow_printf("\r\nInput a new value (1-%d):", sides);
    int new_value = read_int(sides);
    if (new_value > 0) slow_printf(" %d ", new_value);
    int res = candidate + pow(sides, power) * new_value;
    slow_printf(" - buffer %d + (%d ^ %d) * %d = %d", candidate, sides, power, new_value, res);
    Serial.print(" accumulator "); Serial.println(res);
    candidate = res;
  }
  // last can be repeated until valid
  Serial.println("Last value!");
  slow_printf("\r\nLast value!");
  while (true) {
    String message;
    message = String("Input a new value (1-") + sides + String("): ");
    Serial.println(message);
    slow_printf("\r\nInput a new value (1-%d):", sides);
    int new_value = read_int(sides);
    if (new_value > 0) slow_printf(" %d ", new_value);
    int res = candidate + pow(sides, r-1) * new_value;
    slow_printf(" - buffer %d + (%d ^ %d) * %d = %d", candidate, sides, r-1, new_value, res);
    Serial.print(" accumulator "); Serial.println(res);
    if (res < WORDS_NUMBER)
      return res;
    Serial.println("Please retry!");
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
            strIndex[1] = (i == maxIndex) ? i+1 : i;
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
    Terminal.write( (String("\e[")+row+String(";")+col+String("H")).c_str() );   // move cursor
    slow_printf(" %2d %s", x+1, wordn.c_str());
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
  
  int seed_length = 0;
  while (true) {
    Serial.print("Insert seed lenght (12-15-18-21-24): ");
    slow_printf("\r\nInsert seed lenght (12-15-18-21-24): ");
    int new_integer = 0;
    while (new_integer == 0) {
      new_integer = Serial.parseInt();
      if (new_integer > 0)  slow_printf(" %d ", new_integer);
    }
    if ((new_integer == 12) || (new_integer == 15) || (new_integer == 18) || (new_integer == 21) || (new_integer == 24)) {
      seed_length = new_integer;
      Serial.println("");
      break;
    }
    Serial.println("Please retry!");
    slow_printf("\r\nPlease retry!");
  }

  bool use_cards;
  while (true) {
    Serial.print("How you will generate randomness (1-dice, 2-cards): ");
    slow_printf("\r\nHow you will generate randomness (1-dice, 2-cards): ");
    int new_integer = 0;
    while (new_integer == 0) {
      new_integer = Serial.parseInt();
      if (new_integer > 0)  slow_printf(" %d ", new_integer);
    }
    if (new_integer == 1) {
      use_cards = false;
      break;
    } else if (new_integer == 2) {
      use_cards = true;
      Serial.println("");
      break;
    }
    Serial.println("Please retry!");
    slow_printf("\r\nPlease retry!");
  }

  int sides = 0;
  if (use_cards) {
    sides = 52;
  } else {
    while (true) {
      Serial.print("Input sides of the dice (1-64): ");
      slow_printf("\r\nInput sides of the dice (1-64): ");
      int new_integer = 0;
      while (new_integer == 0) {
        new_integer = Serial.parseInt();
        if (new_integer > 0)  slow_printf(" %d ", new_integer);
      }
      if ((new_integer > 0) && (new_integer < 65)) {
        sides = new_integer;
        Serial.println("");
        break;
      }
      Serial.println("Please retry!");
      slow_printf("\r\nPlease retry!");
   }
  }  
  
  struct words * words_english;
  int res = bip39_get_wordlist(NULL, &words_english);

  String mnemonic = "";

  for (int x = 0; x < seed_length - 1; x++) {
    print_header();

    Serial.print("word "); Serial.println(x + 1);
    slow_printf("Partial mnemonic: %s", mnemonic.c_str());
    slow_printf("\r\n================================================================================\r\n");
    slow_printf("Word: %d", x + 1);
    slow_printf("\r\n================================================================================\r\n");
    if (use_cards) 
      print_deck();
    
    int word_index = 0;
    word_index = uniform_generator(WORDS_NUMBER, sides);

    char * new_word;
    res = bip39_get_word(words_english, word_index, &new_word);

    if (x == 0)
      mnemonic = new_word;
    else
      mnemonic = mnemonic + " " + new_word;
    
    Serial.print("word_index "); Serial.println(word_index);
    Serial.println(mnemonic);
    slow_printf("\r\n================================================================================\r\n");
    slow_printf("adding words %s (%d) to the mnemonic", new_word, word_index);
     slow_printf("\r\n================================================================================\r\n");
    delay(1000);
    res = wally_free_string(new_word);
  }

  print_header();

  Serial.print("word "); Serial.println(seed_length);
  slow_printf("Word: %d (last)", seed_length);
  slow_printf("\r\n================================================================================\r\n");
  int check_bits = round(((log10(WORDS_NUMBER) / log10(2)) * seed_length) / 32);
  int value_bits = (log10(WORDS_NUMBER) / log10(2)) - check_bits;
  // get last bits
  int partial_index = 0;
  Serial.print(" get partial_index 0-"); Serial.println(pow(2, round(value_bits)));
  partial_index = uniform_generator(pow(2, round(value_bits)), sides);
  Serial.print("partial_index "); Serial.println(partial_index);
  int prefix = partial_index * pow(2, check_bits);
  Serial.print("prefix "); Serial.println(prefix);

  for (int x = 0; x < pow(2, check_bits); x++) {
    char * new_word;
    res = bip39_get_word(words_english, prefix + x, &new_word);
    String finalized_mnemonic = mnemonic + " " + new_word;
    res = bip39_mnemonic_validate(words_english, finalized_mnemonic.c_str());
    Serial.print(x);
    Serial.print(" ");
    Serial.print(res);
    Serial.print(" ");
    if (res == WALLY_OK) {
      Serial.println(finalized_mnemonic);
      print_header();
      wally_free_string(new_word);
      print_mnemonic(seed_length, finalized_mnemonic, 4);
      slow_printf("\r\n================================================================================\r\n"); 
      break;
    }

  }
  Serial.println("END");
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
  while(true) { 
    delay(1000);
  }
}
