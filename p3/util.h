#ifndef _UTIL_H_
#define _UTIL_H_

struct request{
    int seat_id;
    int user_id;
    int customer_priority;
    char* resource;
};

void parse_request(int, struct request*);
void process_request(int, struct request*);

#endif
