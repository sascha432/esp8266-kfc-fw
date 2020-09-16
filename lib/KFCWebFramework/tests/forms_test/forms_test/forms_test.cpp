
//#include <misc.h>
//#include <array>
//#include <stl_ext.h>
//#include <assert.h>
//#include <PrintString.h>
//#include <PrintHtmlEntitiesString.h>
//#include "KFCForms.h"
//#include "EnumBase.h"
//#include "EnumBitset.h"
//
//#include "Form/Compat.h"




#include <Arduino_compat.h>

void test_form_01_cpp();
void test_form_02_cpp();

int main() {

    ESP._enableMSVCMemdebug();
    DEBUG_HELPER_INIT();

    test_form_02_cpp();
        

    return 0;
}

