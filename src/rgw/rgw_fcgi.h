#ifndef CEPH_RGW_FCGI_H
#define CEPH_RGW_FCGI_H

#include "rgw_client_io.h"


struct FCGX_Request;


class RGWFCGX : public RGWClientIO
{
  FCGX_Request *fcgx;
protected:
  void init_env(CephContext *cct);
  int write_data(const char *buf, int len);
  int read_data(char *buf, int len);

  int send_status(const char *status, const char *status_name);
  int send_100_continue();
  int complete_header();

public:
  RGWFCGX(FCGX_Request *_fcgx) : fcgx(_fcgx) {}
  void flush();
};


#endif
