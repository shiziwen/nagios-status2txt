nagios-status2txt
=================

nagios enhancement to get service data as txt format

1. Overview
status-txt is a Nagios enhancement that duplicates the output of status.cgi in
TXT format to make it easier for other applications/services to use Nagios data.

Compile status-txt.c to create status-txt.cgi, status-txt.cgi accepts exactly
the same URL parameters that status.cgi accepts, and filters its output
appropriately. 
In this version, it accepts almost all the arguments passed in the URL which
status.cgi accepts except "style", "navbarsearch", "columns" and "noheader". 
And we will add more functionalities in the next version.

The status-txt.cgi will returns service data that appears in the "status" table
of status.cgi as "host_ip   host_status   [service:service_status;......]" ,
not includes the top portion of the page, eg:
   196.0.0.1      UP    HTTP_80:OK;
   196.0.0.2      UP    DB_PORT_3303:OK;DB_PORT_3304:OK;
And you can use some arguments to filters its output, such as "hostgroup", 
"servicegroup", "host" and so on.

2. Install
1).Put status-txt.c in nagios/cgi
2).modified the Makefile to add status-txt.c.
3).make && make install

3. Usage
You can use "IP/nagios/cgi-bin/status-txt.cgi" to get the host and status form
nagios. In order to filter its output, you can use some arguments, eg:
"IP/nagios/cgi-bin/status-txt.cgi?host=196.0.0.1".

