#include <pebble.h>
#include <limits.h>
#include "fixed.h"

#define MAX_LENGTH 11

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

static int8_t selected_button = 13; //Currently selected button, starts on '5'

static bool operator_entered = false; //Has the operator been entered yet
//static bool entering_decimal = false; //Is the user currently entering a decimal value
static bool num1_is_ans = false; //Is the previous result in num1? Used to allow further calculations on result.
static char num1[MAX_LENGTH] = ""; //First operand
static char num2[MAX_LENGTH] = ""; //Second operand
static char result_text[MAX_LENGTH] = ""; //Results text layer buffer string
/*static char *num1 = num1_a;
static char *num2 = num2_a;
static char *result_text = result_text_a;*/
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

static void enter(){
  char *num = operator_entered ? num2 : num1; //Create a pointer to the currnetly edited number
  strcat(num, buttons[selected_button]);
  text_layer_set_text(s_result_text_layer, num); //Display num
}

static void enter_operator(uint8_t id){
  operator = id; //Set operator to operator id
  operator_entered = true;
  text_layer_set_text(s_result_text_layer, buttons[selected_button]); //Display operator
}

static void clear(bool full){
  char *num = operator_entered ? num2 : num1;
  if(full) 
    *num = 0; 
  else 
    num[strlen(num)-1] = 0;
  text_layer_set_text(s_result_text_layer, num);
}

static void switch_sign(){
  bool overflow = false; //Overflow flag
  char *str_num = operator_entered ? num2 : num1; //Get pointer to currently edited number
  fixed num = str_to_fixed(str_num, &overflow);
  num = fixed_mult(num, int_to_fixed(-1), &overflow); //Multiply by -1
  if(!overflow){
    fixed_repr(num, str_num, MAX_LENGTH);
    text_layer_set_text(s_result_text_layer, str_num); //Display number
  }
}

static void calculate(){
  bool overflow = false; //Overflow flag
  fixed lhs = str_to_fixed(num1, &overflow);
  fixed rhs = str_to_fixed(num2, &overflow);
  fixed result = 0;
  //Calculate the result
  switch(operator){
    case 0:
      result = fixed_add(lhs, rhs, &overflow);
      break;
    case 1:
      result = fixed_subt(lhs, rhs, &overflow);
      break;
    case 2:
      result = fixed_mult(lhs, rhs, &overflow);
      break;
    case 3:
      result = fixed_div(lhs, rhs);
      break;
    case 4:
      result = fixed_pow(lhs, fixed_to_int(rhs), &overflow); //Exponent must be an int
      break;
    default:
      result = 0;
  }

  APP_LOG(APP_LOG_LEVEL_DEBUG, "num1: %d num2: %d result: %d", lhs, rhs, result);

  //Reset operands, operator_entered and entering_decimal
  *num1 = 0;
  *num2 = 0;
  operator_entered = false;
  
  if(overflow){
    text_layer_set_text(s_result_text_layer, "Overflow Error"); //Display message on overflow
  }
  else{
    fixed_repr(result, result_text, MAX_LENGTH);
    text_layer_set_text(s_result_text_layer, result_text); //Display result
    strcpy(num1, result_text);
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
    case 5:// ^
      enter_operator(4);
      break;
    case 6:// C
      clear(false);
      break;
    case 7:// CE
      clear(true);
      break;
    case 11:// +-
      switch_sign();
      break;
    case 19:// =
      calculate();
      break;
    default:
      enter();
  }
}

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
	text_layer_set_text(s_result_text_layer, "");
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

int main(void) {
	init();
	app_event_loop();
	deinit();
}
