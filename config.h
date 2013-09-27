//
// config.h
// user configuration
//
static struct binding bindings[] = {
	// modifier              key               action         callback         argument
	{GLFW_MOD_CONTROL,       GLFW_KEY_F,       GLFW_PRESS,    createFrame,     { 0 }},
	{GLFW_MOD_CONTROL,       GLFW_KEY_W,       GLFW_PRESS,    saveCopy,        { 0 }},
	{GLFW_MOD_CONTROL,       GLFW_KEY_S,       GLFW_PRESS,    save,            { 0 }},
	{0,                      '.',              GLFW_PRESS,    zoom,            { .i = +1 }},
	{0,                      ',',              GLFW_PRESS,    zoom,            { .i = -1 }},
	{0,                      ']',              GLFW_PRESS,    brushSize,       { .i = +1 }},
	{0,                      '[',              GLFW_PRESS,    brushSize,       { .i = -1 }},
	{0,                      GLFW_KEY_U,       GLFW_PRESS,    undo,            { 0 }},
	{GLFW_MOD_CONTROL,       GLFW_KEY_R,       GLFW_PRESS,    redo,            { 0 }},
	{0,                      GLFW_KEY_ENTER,   GLFW_PRESS,    pause,           { 0 }},
	{0,                      GLFW_KEY_LEFT,    GLFW_PRESS,    move,            { .p = {-50, 0} }},
	{0,                      GLFW_KEY_RIGHT,   GLFW_PRESS,    move,            { .p = {+50, 0} }},
	{0,                      GLFW_KEY_DOWN,    GLFW_PRESS,    move,            { .p = {0, +50} }},
	{0,                      GLFW_KEY_UP,      GLFW_PRESS,    move,            { .p = {0, -50} }},
	{0,                      GLFW_KEY_SPACE,   GLFW_PRESS,    pan,             { true }},
	{0,                      GLFW_KEY_SPACE,   GLFW_RELEASE,  pan,             { false }},
	{0,                      GLFW_KEY_ESCAPE,  GLFW_PRESS,    windowClose,     { 0 }},
	{GLFW_MOD_SHIFT,         GLFW_KEY_EQUAL,   GLFW_PRESS,    adjustFPS,       { .i = +1 }},
	{0,                      GLFW_KEY_MINUS,   GLFW_PRESS,    adjustFPS,       { .i = -1 }}
};
