#ifndef __MAIN_AWS_H__
#define __MAIN_AWS_H__

void init_aws(void);
void GSM_CONNECTED(void);
void GSM_DISCONNECTED(void);
void AWS_loop(void);
void  publish(void);
void aws_client_loop(void);

#endif