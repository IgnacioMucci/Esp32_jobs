#ifndef PERIOD_HANDLERS_H
#define PERIOD_HANDLERS_H

extern uint64_t time_deepsleep;

//Sets the amount of time that the device goes to deepsleep mode
void setterPeriod(int sockfd, char *argument);
//Gets the amount of time thats configured
void getterPeriod(int sockfd, char *argument);

#endif