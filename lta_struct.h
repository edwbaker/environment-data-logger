struct lta {
 uint32_t time;
 float humidity;
 float temperature;
 boolean ethernet;
 boolean fail;
};

struct error {
  String condition;
  int level;
};

/**
LEVEL
1 - FATAL
2 - ERROR
3 - WARNING
4 - NOTICE
*/
