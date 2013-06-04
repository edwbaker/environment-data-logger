struct lta {
 float humidity;
 float temperature;
 boolean ethernet;
 boolean fail;
};

struct error {
  //Error condition
  int condition;
    // 1   - DHT read error
    // 2   - Ethernet POST fail
    // 3   - SD card initalisation error
    // 4   - data.txt not opened
  int level;
    // 1   - warning (system will try to correct. flash once)
    // 2   - fatal (soft reset) 
};
