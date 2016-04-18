#include <pebble.h>
#include <limits.h>
#include "fixed.h"

//Layers
static Window *s_window;
static TextLayer *s_result_text_layer;
static Layer *s_buttons_layer;

//Button labels
static char *buttons[] = {
  "+","-","*","/",
  "0","^","C","CE",
  "1","2","3","+-",
  "4","5","6",".",
  "7","8","9","="
};

static char result_text[] =  "00000000000"; //Results text layer buffer string

static int8_t selected_button = 13; //Currently selected button, starts on '5'

static bool operator_entered = false; //Has the operator been entered yet
static bool entering_decimal = false; //Is the user currently entering a decimal value
static bool num1_is_ans = false; //Is the previous result in num1? Used to allow further calculations on result.
static fixed num1 = 0; //First operand
static fixed num2 = 0; //Second operand
static uint8_t operator = 0; //Operator, where 0 is +, 1 is -, 2 is *, 3 is /, 4 is ^

//Update handler for the buttons layer
static void button_layer_update(Layer *layer, GContext *ctx) {
  //Set up colors
  #if defined(PBL_COLOR)
    graphics_context_set_stroke_color(ctx, GColorCobaltBlue);
    graphics_context_set_fill_color(ctx, GColorOxfordBlue);
  #elif defined(PBL_BW)
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorBlack);
  #endif
  //Set font
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  //Nested loop. 5 rows of 4
  for(int y = 0; y < 5; y++){
    for(int x = 0; x < 4; x++){
      //Define button bounds
      GRect rect_bounds = GRect(2 + (x*36), 40 + (y*26), 32, 22);
      if( (y*4 + x) == selected_button ){ //If currently rendered button is the selected button
        //Change the text color and draw a filled rectangle
        graphics_context_set_text_color(ctx, GColorWhite);
        graphics_fill_rect(ctx, rect_bounds, 2, GCornersAll);
      }
      else { //For all other buttons
        //Use a black text color and draw an empty rectangle
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_round_rect(ctx, rect_bounds, 2);
      }
      //Define text bounds
      GRect text_bounds = GRect(2 + (x*36), 40 + (y*26), 32, 19);
      //Draw the button labels
      graphics_draw_text(ctx, buttons[y*4 + x], font, text_bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
  }
}

//Up or Down button handler
static void up_down_handler(ClickRecognizerRef recognizer, void *context){
  //Move selected button down if down is pressed, and up if up is pressed
  selected_button += (click_recognizer_get_button_id(recognizer) == BUTTON_ID_DOWN) ? 1 : -1;
  //If selected button is outside button range, wrap around
  selected_button = selected_button < 0 ? 19 : selected_button > 19 ? 0 : selected_button;
  //Mark button layer dirty for redraw
  layer_mark_dirty(s_buttons_layer);
}

static void enter_digit(uint8_t digit){
  bool overflow = false; //Overflow flag
  fixed *num = operator_entered ? &num2 : &num1; //Create a pointer to the currnetly edited number
  if(entering_decimal){ //If currently entering decimal part
    int decimal_part = *num % FIXED_SCALE; //Get the decimal part of the number
    fixed factor = decimal_part == 0 ? 10 : ( decimal_part % 10 == 0 ? 01 : 0 ); //Calculate a factor to multiply by based on number of digits after decimal point
    APP_LOG(APP_LOG_LEVEL_DEBUG, "decimal: %d, factor: %d", decimal_part, factor);
    *num = fixed_add(*num, fixed_mult(int_to_fixed(digit), factor, &overflow), &overflow); //Add the entered digit multiplied by the factor
  }
  else{
    *num = fixed_mult(*num, int_to_fixed(10), &overflow); //Multiply num by 10 in order to shift the digits left
    *num = fixed_add(*num, int_to_fixed(digit), &overflow); //Add the entered digit
  }
  fixed_repr(*num, result_text, sizeof(result_text)); //Convert num to string
  text_layer_set_text(s_result_text_layer, result_text); //Display num
}

static void enter_operator(uint8_t id){
  operator = id; //Set operator to operator id
  operator_entered = true;
  entering_decimal = false; //Reset entering_decimal
  text_layer_set_text(s_result_text_layer, buttons[selected_button]); //Display operator
}

static void clear(bool full){
  fixed *num = operator_entered ? &num2 : &num1;
  if(full){
    *num = 0;
  }
  else{
    if(entering_decimal && *num % FIXED_SCALE == 0){ //If entering decimal, but no decimal digits are currently entered
      entering_decimal = false;
    }
    else{
      *num -= *num % FIXED_SCALE; //Remove last digit
      *num /= 10; //Shift all digits right
    }
  }
  fixed_repr(*num, result_text, sizeof(result_text));
  text_layer_set_text(s_result_text_layer, result_text);
}

static void switch_sign(){
  bool overflow = false; //Overflow flag
  fixed *num = operator_entered ? &num2 : &num1; //Get pointer to currently edited number
  *num = fixed_mult(*num, int_to_fixed(-1), &overflow); //Multiply by -1
  fixed_repr(*num, result_text, sizeof(result_text));
  text_layer_set_text(s_result_text_layer, result_text); //Display number
}

static void calculate(){
  fixed result = 0;
  bool overflow = false; //Overflow flag
  //Calculate the result
  switch(operator){
    case 0:
      result = fixed_add(num1, num2, &overflow);
      break;
    case 1:
      result = fixed_subt(num1, num2, &overflow);
      break;
    case 2:
      result = fixed_mult(num1, num2, &overflow);
      break;
    case 3:
      result = fixed_div(num1, num2);
      break;
    case 4:
      result = fixed_pow(num1, fixed_to_int(num2), &overflow); //Exponent must be an int
      break;
    default:
      result = 0;
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "num1: %d num2: %d result: %d", num1, num2, result);

  //Reset operands, operator_entered and entering_decimal
  num1 = 0;
  num2 = 0;
  operator_entered = false;
  entering_decimal = false;
  
  if(overflow){
    text_layer_set_text(s_result_text_layer, "Overflow Error"); //Display message on overflow
  }
  else{
    //snprintf(result_text, sizeof(result_text), "%li", result); //Convert result to string
    fixed_repr(result, result_text, sizeof(result_text));
    text_layer_set_text(s_result_text_layer, result_text); //Display result
    //Put result in num1
    APP_LOG(APP_LOG_LEVEL_DEBUG, "result: %d", result);
    num1 = result;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "num1: %d", num1);
    num1_is_ans = true;
  }
}

//Button press handler
static void select_handler(ClickRecognizerRef recognizer, void *context){
  switch(selected_button){
    case 0: // +
      enter_operator(0);
      break;
    case 1: // -
      enter_operator(1);
      break;
    case 2: // *
      enter_operator(2);
      break;
    case 3: // /
      enter_operator(3);
      break;
    case 4:// 0
      enter_digit(0);
      break;
    case 5:// ^
      enter_operator(4);
      break;
    case 6:// C
      clear(false);
      break;
    case 7:// CE
      clear(true);
      break;
    case 8:// 1
      enter_digit(1);
      break;
    case 9:// 2
      enter_digit(2);
      break;
    case 10:// 3
      enter_digit(3);
      break;
    case 11:// +-
      switch_sign();
      break;
    case 12:// 4
      enter_digit(4);
      break;
    case 13:// 5
      enter_digit(5);
      break;
    case 14:// 6
      enter_digit(6);
      break;
    case 15:// .
      entering_decimal = true;
      break;
    case 16:// 7
      enter_digit(7);
      break;
    case 17:// 8
      enter_digit(8);
      break;
    case 18:// 9
      enter_digit(9);
      break;
    case 19:// =
      calculate();
      break;
  }
}

/*
//Select button pressed handler
static void select_handler(ClickRecognizerRef recognizer, void *context){
  //If the operator hasn't been entered yet
  if (!operator_entered){
    //If a number is pressed
    if (selected_button >= 5) {
      if(num1_is_ans){
        //If num1 contains the previous result, num1 should be reset before entering the number
        num1 = 0;
        num1_is_ans = false;
      }
      
      enter_digit(&num1);
    }
    //If an operator is pressed
    else if (selected_button < 4) {
      operator = selected_button; //Save the operator pressed
      operator_entered = true;
      text_layer_set_text(s_result_text_layer, buttons[selected_button]); //Display the operator
    }
  }
  //If the operator has already been entered
  else {
    //If a number is entered
    if (selected_button >= 5) {
      enter_digit(&num2);
    }
    //If the equals sign is pressed
    else if (selected_button == 4) {
      fixed result = 0;
      bool overflow = false;
      //Calculate the result
      switch(operator){
        case 0:
          result = fixed_div(num1, num2);
          break;
        case 1:
          result = fixed_mult(num1, num2, &overflow);
          break;
        case 2:
          result = fixed_add(num1, num2, &overflow);
          break;
        case 3:
          result = fixed_subt(num1, num2, &overflow);
          break;
        default:
          result = 0;
      }
      
      APP_LOG(APP_LOG_LEVEL_DEBUG, "num1: %d num2: %d result: %d", num1, num2, result);
      
      if(overflow){
        text_layer_set_text(s_result_text_layer, "Overflow Error");
      }
      else{
        //snprintf(result_text, sizeof(result_text), "%li", result); //Convert result to string
        fixed_repr(result, result_text, sizeof(result_text));
        text_layer_set_text(s_result_text_layer, result_text); //Display result
        //Put result in num1
        APP_LOG(APP_LOG_LEVEL_DEBUG, "result: %d", result);
        num1 = result;
        APP_LOG(APP_LOG_LEVEL_DEBUG, "num1: %d", num1);
        num1_is_ans = true;
      }
      
      //Reset operands and operator_entered
      num2 = 0;
      operator_entered = false;
    }
  }
}*/

static void click_config_provider(void *context) {
  //Register click handlers
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_down_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, up_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
}

static void init(void) {
	// Create a window and get information about the window
	s_window = window_create();
  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  //Register click config provider
  window_set_click_config_provider(s_window, click_config_provider);
	
  // Result text layer setup
  GRect text_bounds = GRect(6, 6, 132, 32);
	s_result_text_layer = text_layer_create(text_bounds);
	text_layer_set_text(s_result_text_layer, "0");
	text_layer_set_font(s_result_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(s_result_text_layer, GTextAlignmentRight);
	layer_add_child(window_get_root_layer(s_window), text_layer_get_layer(s_result_text_layer));
  
  //Button layer setup
  s_buttons_layer = layer_create(bounds);
  layer_set_update_proc(s_buttons_layer, button_layer_update);
  layer_add_child(window_get_root_layer(s_window), s_buttons_layer);

	// Push the window, setting the window animation to 'true'
	window_stack_push(s_window, true);
}

static void deinit(void) {
	// Destroy the text layer
	text_layer_destroy(s_result_text_layer);
  layer_destroy(s_buttons_layer);
	
	// Destroy the window
	window_destroy(s_window);
}

/*int main(void) {
	init();
	app_event_loop();
	deinit();
}*/
