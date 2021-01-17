/**
 * Author: sascha_lammers@gmx.de
 */

// --------------------------------------------------------------------
auto &skipRowsGroups = form.addCardGroup(F("src"), F("Skip Rows Or Columns"), true);

form.addPointerTriviallyCopyable(F("skrows"), &cfg.skip_rows.rows);
form.addFormUI(F("Display every nth row"));
form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_ROWS));

form.addPointerTriviallyCopyable(F("skcols"), &cfg.skip_rows.cols);
form.addFormUI(F("Display every nth column"));
form.addValidator(FormUI::Validator::Range(0, IOT_LED_MATRIX_COLS));

form.addPointerTriviallyCopyable(F("sktime"), &cfg.skip_rows.time);
form.addFormUI(F("Rotate Through Rows And Columns"), FormUI::Suffix(F("milliseconds, 0 = disable")));
