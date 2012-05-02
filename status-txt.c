/**************************************************************************
 *
 * STATUS-TXT.C -  Nagios Status CGI
 *
 * Copyright (c) 1999-2009 Ethan Galstad (egalstad@nagios.org)
 * Last Modified: 04-27-2012
 * MM: Modified version of status CGI to provide TXT
 *
 * License:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *************************************************************************/

#include "../include/config.h"
#include "../include/common.h"
#include "../include/objects.h"
#include "../include/comments.h"
#include "../include/macros.h"
#include "../include/statusdata.h"

#include "../include/cgiutils.h"
#include "../include/getcgi.h"
#include "../include/cgiauth.h"

extern int             refresh_rate;
extern time_t          program_start;

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_docs_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char url_media_path[MAX_FILENAME_LENGTH];
extern char log_file[MAX_FILENAME_LENGTH];

extern char *notes_url_target;
extern char *action_url_target;

extern int suppress_alert_window;

extern int enable_splunk_integration;

extern host *host_list;
extern service *service_list;
extern hostgroup *hostgroup_list;
extern servicegroup *servicegroup_list;
extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;


#define MAX_MESSAGE_BUFFER		4096

#define DISPLAY_HOSTS			0
#define DISPLAY_HOSTGROUPS		1
#define DISPLAY_SERVICEGROUPS           2


/* HOSTSORT structure */
typedef struct hostsort_struct{
	hoststatus *hststatus;
	struct hostsort_struct *next;
        }hostsort;

/* SERVICESORT structure */
typedef struct servicesort_struct{
	servicestatus *svcstatus;
	struct servicesort_struct *next;
        }servicesort;

hostsort *hostsort_list=NULL;
servicesort *servicesort_list=NULL;

int sort_services(int,int);						/* sorts services */
int compare_servicesort_entries(int,int,servicesort *,servicesort *);	/* compares service sort entries */
void free_servicesort_list(void);
void free_hostsort_list(void);

void show_service_detail(void);

int passes_host_properties_filter(hoststatus *);
int passes_service_properties_filter(servicestatus *);

void document_header(int);
void document_footer(void);
int process_cgivars(void);


authdata current_authdata;
time_t current_time;

char *host_name=NULL;
char *host_filter=NULL;
char *hostgroup_name=NULL;
char *servicegroup_name=NULL;
char *service_filter=NULL;
int show_all_hosts=TRUE;
int show_all_hostgroups=TRUE;
int show_all_servicegroups=TRUE;
int display_type=DISPLAY_HOSTS;
int overview_columns=3;
int max_grid_width=8;

int service_status_types=SERVICE_PENDING|SERVICE_OK|SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;
int all_service_status_types=SERVICE_PENDING|SERVICE_OK|SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;

int host_status_types=HOST_PENDING|HOST_UP|HOST_DOWN|HOST_UNREACHABLE;
int all_host_status_types=HOST_PENDING|HOST_UP|HOST_DOWN|HOST_UNREACHABLE;

int all_service_problems=SERVICE_UNKNOWN|SERVICE_WARNING|SERVICE_CRITICAL;
int all_host_problems=HOST_DOWN|HOST_UNREACHABLE;

unsigned long host_properties=0L;
unsigned long service_properties=0L;

int sort_type=SORT_NONE;
int sort_option=SORT_HOSTNAME;

int embedded=FALSE;


int main(void){
	int result=OK;
	host *temp_host=NULL;
	hostgroup *temp_hostgroup=NULL;
	servicegroup *temp_servicegroup=NULL;
	
	time(&current_time);

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result=read_cgi_config_file(get_cgi_config_location());
	if(result==ERROR){
		document_header(FALSE);
		cgi_config_file_error(get_cgi_config_location());
		document_footer();
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		document_header(FALSE);
		main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_ALL_OBJECT_DATA);
	if(result==ERROR){
		document_header(FALSE);
		object_data_error();
		document_footer();
		return ERROR;
                }

	/* read all status data */
	result=read_all_status_data(get_cgi_config_location(),READ_ALL_STATUS_DATA);
	if(result==ERROR){
		document_header(FALSE);
		status_data_error();
		document_footer();
		free_memory();
		return ERROR;
                }

	/* initialize macros */
	init_macros();

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* bottom portion of screen - service or hostgroup detail */
	show_service_detail();

	document_footer();

	/* free all allocated memory */
	free_memory();
	free_comment_data();

	/* free memory allocated to the sort lists */
	free_servicesort_list();
	free_hostsort_list();

	return OK;
        }


void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t expire_time;

	printf("Cache-Control: no-store\r\n");
	printf("Pragma: no-cache\r\n");
	printf("Refresh: %d\r\n",refresh_rate);

	get_time_string(&current_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\r\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\r\n",date_time);

	printf("Content-type: text/plain\r\n\r\n");

	if(embedded==TRUE)
		return;

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	return;
        }


int process_cgivars(void){
	char **variables;
	int error=FALSE;
	int x;

	variables=getcgivars();

	for(x=0;variables[x]!=NULL;x++){

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
			x++;
			continue;
		        }


		/* we found the hostgroup argument */
		else if(!strcmp(variables[x],"hostgroup")){
			display_type=DISPLAY_HOSTGROUPS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			hostgroup_name=(char *)strdup(variables[x]);
			strip_html_brackets(hostgroup_name);

			if(hostgroup_name!=NULL && !strcmp(hostgroup_name,"all"))
				show_all_hostgroups=TRUE;
			else
				show_all_hostgroups=FALSE;
		        }

		/* we found the servicegroup argument */
		else if(!strcmp(variables[x],"servicegroup")){
			display_type=DISPLAY_SERVICEGROUPS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			servicegroup_name=strdup(variables[x]);
			strip_html_brackets(servicegroup_name);

			if(servicegroup_name!=NULL && !strcmp(servicegroup_name,"all"))
				show_all_servicegroups=TRUE;
			else
				show_all_servicegroups=FALSE;
		        }

		/* we found the host argument */
		else if(!strcmp(variables[x],"host")){
			display_type=DISPLAY_HOSTS;
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_name=strdup(variables[x]);
			strip_html_brackets(host_name);

			if(host_name!=NULL && !strcmp(host_name,"all"))
				show_all_hosts=TRUE;
			else
				show_all_hosts=FALSE;
		        }

		/* we found the service status type argument */
		else if(!strcmp(variables[x],"servicestatustypes")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_status_types=atoi(variables[x]);
		        }

		/* we found the host status type argument */
		else if(!strcmp(variables[x],"hoststatustypes")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_status_types=atoi(variables[x]);
		        }

		/* we found the service properties argument */
		else if(!strcmp(variables[x],"serviceprops")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_properties=strtoul(variables[x],NULL,10);
		        }

		/* we found the host properties argument */
		else if(!strcmp(variables[x],"hostprops")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_properties=strtoul(variables[x],NULL,10);
		        }

		/* we found the host or service group style argument (do not support this argument)*/
		
		/* we found the sort type argument */
		else if(!strcmp(variables[x],"sorttype")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			sort_type=atoi(variables[x]);
		        }

		/* we found the sort option argument */
		else if(!strcmp(variables[x],"sortoption")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			sort_option=atoi(variables[x]);
		        }

		/* we found the embed option */
		else if(!strcmp(variables[x],"embedded"))
			embedded=TRUE;

		/* servicefilter cgi var */
                else if(!strcmp(variables[x],"servicefilter")){
                        x++;
                        if(variables[x]==NULL){
                                error=TRUE;
                                break;
                                }
                        service_filter=strdup(variables[x]);
			strip_html_brackets(service_filter);
                        }
	        }

	/* free memory allocated to the CGI variables */
	free_cgivars(variables);

	return error;
        }


/* display a rough listing of the status of all services... */
void show_service_detail(void){
	regex_t preg, preg_hostname; 
	time_t t;
	char date_time[MAX_DATETIME_LENGTH];
	char status[MAX_INPUT_BUFFER];
	char host_status[MAX_INPUT_BUFFER];
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_url[MAX_INPUT_BUFFER];
	char *last_host="";
	int new_host=FALSE;
	servicestatus *temp_status=NULL;
	hostgroup *temp_hostgroup=NULL;
	servicegroup *temp_servicegroup=NULL;
	hoststatus *temp_hoststatus=NULL;
	host *temp_host=NULL;
	service *temp_service=NULL;
	int total_comments=0;
	servicesort *temp_servicesort=NULL;
	int use_sort=FALSE;
	int result=OK;
	int first_entry=TRUE;
	int first_host=TRUE;
	int total_entries=0;
	int show_service=FALSE;


	/* sort the service list if necessary */
	if(sort_type!=SORT_NONE){
		result=sort_services(sort_type,sort_option);
		if(result==ERROR)
			use_sort=FALSE;
		else
			use_sort=TRUE;
	}
	else
		use_sort=FALSE;

	if(service_filter!=NULL)
		regcomp(&preg,service_filter,0);
	if(host_filter!=NULL)
		regcomp(&preg_hostname,host_filter,REG_ICASE);

	temp_hostgroup=find_hostgroup(hostgroup_name);
	temp_servicegroup=find_servicegroup(servicegroup_name);

	/* check all services... */
	while(1){

		/* get the next service to display */
		if(use_sort==TRUE){
			if(first_entry==TRUE)
				temp_servicesort=servicesort_list;
			else 
				temp_servicesort=temp_servicesort->next;
			if(temp_servicesort==NULL)
				break;
			temp_status=temp_servicesort->svcstatus;
		}
		else{
			if(first_entry==TRUE)
				temp_status=servicestatus_list;
			else
				temp_status=temp_status->next;
		}

		if(temp_status==NULL)
			break;

		first_entry=FALSE;

		/* find the service  */
		temp_service=find_service(temp_status->host_name,temp_status->description);

		/* if we couldn't find the service, go to the next service */
		if(temp_service==NULL)
			continue;

		/* find the host */
		temp_host=find_host(temp_service->host_name);

		/* make sure user has rights to see this... */
		if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
			continue;

		/* get the host status information */
		temp_hoststatus=find_hoststatus(temp_service->host_name);

		/* see if we should display services for hosts with tis type of status */
		if(!(host_status_types & temp_hoststatus->status))
			continue;

		/* see if we should display this type of service status */
		if(!(service_status_types & temp_status->status))
			continue;	

		/* check host properties filter */
		if(passes_host_properties_filter(temp_hoststatus)==FALSE)
			continue;

		/* check service properties filter */
		if(passes_service_properties_filter(temp_status)==FALSE)
			continue;

		/* servicefilter cgi var */
		if(service_filter!=NULL){
			if(regexec(&preg,temp_status->description,0,NULL,0)){
				continue;
			}
		}

		show_service=FALSE;

		if(display_type==DISPLAY_HOSTS){
			if(show_all_hosts==TRUE)
				show_service=TRUE;
			else if(host_filter!=NULL && 0==regexec(&preg_hostname,temp_status->host_name,0,NULL,0))
				show_service=TRUE;
			else if(!strcmp(host_name,temp_status->host_name))
				show_service=TRUE;
		}

		else if(display_type==DISPLAY_HOSTGROUPS){
			if(show_all_hostgroups==TRUE)
				show_service=TRUE;
			else if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==TRUE)
				show_service=TRUE;
		}

		else if(display_type==DISPLAY_SERVICEGROUPS){
			if(show_all_servicegroups==TRUE)
				show_service=TRUE;
			else if(is_service_member_of_servicegroup(temp_servicegroup,temp_service)==TRUE)
				show_service=TRUE;
		}

		if(show_service==TRUE){
			if(strcmp(last_host,temp_status->host_name))
				new_host=TRUE;
			else
				new_host=FALSE;

			/* host name column */
			if(new_host==TRUE){

				/* grab macros */
				grab_host_macros(temp_host);

				if(temp_hoststatus->status==HOST_DOWN){
					strncpy(host_status,"DOWN",sizeof(host_status));
				}
				else if(temp_hoststatus->status==HOST_UNREACHABLE){
					strncpy(host_status,"UNREACHABLE",sizeof(host_status));
				}
				else if(temp_hoststatus->status==HOST_UP){
					strncpy(host_status,"UP",sizeof(host_status));
				}
				else if(temp_hoststatus->status==HOST_PENDING){
					strncpy(host_status,"PENDING",sizeof(host_status));
				}
				host_status[sizeof(host_status)-1]='\x0';

				if(first_host==FALSE){
					printf("\n");
				}
				first_host=FALSE;
				printf("%s\t",temp_status->host_name);
				printf("%s\t",host_status);
			}

			/* keep track of total number of services we're displaying */
			total_entries++;

			/* get service status */
			if(temp_status->status==SERVICE_PENDING){
				strncpy(status,"PENDING",sizeof(status));
			}
			else if(temp_status->status==SERVICE_OK){
				strncpy(status,"OK",sizeof(status));
			}
			else if(temp_status->status==SERVICE_WARNING){
				strncpy(status,"WARNING",sizeof(status));
			}
			else if(temp_status->status==SERVICE_UNKNOWN){
				strncpy(status,"UNKNOWN",sizeof(status));
			}
			else if(temp_status->status==SERVICE_CRITICAL){
				strncpy(status,"CRITICAL",sizeof(status));
			}
			status[sizeof(status)-1]='\x0';


			/* grab macros */
			grab_service_macros(temp_service);

			/* service description and status */
			printf("%s:", temp_service->description);
			printf("%s;", status);

			last_host=temp_status->host_name;
		} // show service

	} // while(1)

	return;
}


/******************************************************************/
/**********  SERVICE SORTING & FILTERING FUNCTIONS  ***************/
/******************************************************************/


/* sorts the service list */
int sort_services(int s_type, int s_option){
	servicesort *new_servicesort;
	servicesort *last_servicesort;
	servicesort *temp_servicesort;
	servicestatus *temp_svcstatus;

	if(s_type==SORT_NONE)
		return ERROR;

	if(servicestatus_list==NULL)
		return ERROR;

	/* sort all services status entries */
	for(temp_svcstatus=servicestatus_list;temp_svcstatus!=NULL;temp_svcstatus=temp_svcstatus->next){

		/* allocate memory for a new sort structure */
		new_servicesort=(servicesort *)malloc(sizeof(servicesort));
		if(new_servicesort==NULL)
			return ERROR;

		new_servicesort->svcstatus=temp_svcstatus;

		last_servicesort=servicesort_list;
		for(temp_servicesort=servicesort_list;temp_servicesort!=NULL;temp_servicesort=temp_servicesort->next){

			if(compare_servicesort_entries(s_type,s_option,new_servicesort,temp_servicesort)==TRUE){
				new_servicesort->next=temp_servicesort;
				if(temp_servicesort==servicesort_list)
					servicesort_list=new_servicesort;
				else
					last_servicesort->next=new_servicesort;
				break;
		                }
			else
				last_servicesort=temp_servicesort;
	                }

		if(servicesort_list==NULL){
			new_servicesort->next=NULL;
			servicesort_list=new_servicesort;
	                }
		else if(temp_servicesort==NULL){
			new_servicesort->next=NULL;
			last_servicesort->next=new_servicesort;
	                }
	        }

	return OK;
        }


int compare_servicesort_entries(int s_type, int s_option, servicesort *new_servicesort, servicesort *temp_servicesort){
	servicestatus *new_svcstatus;
	servicestatus *temp_svcstatus;
	time_t nt;
	time_t tt;

	new_svcstatus=new_servicesort->svcstatus;
	temp_svcstatus=temp_servicesort->svcstatus;

	if(s_type==SORT_ASCENDING){

		if(s_option==SORT_LASTCHECKTIME){
			if(new_svcstatus->last_check < temp_svcstatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_CURRENTATTEMPT){
			if(new_svcstatus->current_attempt < temp_svcstatus->current_attempt)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICESTATUS){
			if(new_svcstatus->status <= temp_svcstatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_svcstatus->host_name,temp_svcstatus->host_name)<0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICENAME){
			if(strcasecmp(new_svcstatus->description,temp_svcstatus->description)<0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_STATEDURATION){
			if(new_svcstatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_svcstatus->last_state_change>current_time)?0:(current_time-new_svcstatus->last_state_change);
			if(temp_svcstatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_svcstatus->last_state_change>current_time)?0:(current_time-temp_svcstatus->last_state_change);
			if(nt<tt)
				return TRUE;
			else
				return FALSE;
		        }
	        }
	else{
		if(s_option==SORT_LASTCHECKTIME){
			if(new_svcstatus->last_check > temp_svcstatus->last_check)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_CURRENTATTEMPT){
			if(new_svcstatus->current_attempt > temp_svcstatus->current_attempt)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICESTATUS){
			if(new_svcstatus->status > temp_svcstatus->status)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_HOSTNAME){
			if(strcasecmp(new_svcstatus->host_name,temp_svcstatus->host_name)>0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_SERVICENAME){
			if(strcasecmp(new_svcstatus->description,temp_svcstatus->description)>0)
				return TRUE;
			else
				return FALSE;
		        }
		else if(s_option==SORT_STATEDURATION){
			if(new_svcstatus->last_state_change==(time_t)0)
				nt=(program_start>current_time)?0:(current_time-program_start);
			else
				nt=(new_svcstatus->last_state_change>current_time)?0:(current_time-new_svcstatus->last_state_change);
			if(temp_svcstatus->last_state_change==(time_t)0)
				tt=(program_start>current_time)?0:(current_time-program_start);
			else
				tt=(temp_svcstatus->last_state_change>current_time)?0:(current_time-temp_svcstatus->last_state_change);
			if(nt>tt)
				return TRUE;
			else
				return FALSE;
		        }
	        }

	return TRUE;
        }



/* sorts the host list */





/* free all memory allocated to the servicesort structures */
void free_servicesort_list(void){
	servicesort *this_servicesort;
	servicesort *next_servicesort;

	/* free memory for the servicesort list */
	for(this_servicesort=servicesort_list;this_servicesort!=NULL;this_servicesort=next_servicesort){
		next_servicesort=this_servicesort->next;
		free(this_servicesort);
	        }

	return;
        }


/* free all memory allocated to the hostsort structures */
void free_hostsort_list(void){
	hostsort *this_hostsort;
	hostsort *next_hostsort;

	/* free memory for the hostsort list */
	for(this_hostsort=hostsort_list;this_hostsort!=NULL;this_hostsort=next_hostsort){
		next_hostsort=this_hostsort->next;
		free(this_hostsort);
	        }

	return;
        }



/* check host properties filter */
int passes_host_properties_filter(hoststatus *temp_hoststatus){

	if((host_properties & HOST_SCHEDULED_DOWNTIME) && temp_hoststatus->scheduled_downtime_depth<=0)
		return FALSE;

	if((host_properties & HOST_NO_SCHEDULED_DOWNTIME) && temp_hoststatus->scheduled_downtime_depth>0)
		return FALSE;

	if((host_properties & HOST_STATE_ACKNOWLEDGED) && temp_hoststatus->problem_has_been_acknowledged==FALSE)
		return FALSE;

	if((host_properties & HOST_STATE_UNACKNOWLEDGED) && temp_hoststatus->problem_has_been_acknowledged==TRUE)
		return FALSE;

	if((host_properties & HOST_CHECKS_DISABLED) && temp_hoststatus->checks_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_CHECKS_ENABLED) && temp_hoststatus->checks_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_EVENT_HANDLER_DISABLED) && temp_hoststatus->event_handler_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_EVENT_HANDLER_ENABLED) && temp_hoststatus->event_handler_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_FLAP_DETECTION_DISABLED) && temp_hoststatus->flap_detection_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_FLAP_DETECTION_ENABLED) && temp_hoststatus->flap_detection_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_IS_FLAPPING) && temp_hoststatus->is_flapping==FALSE)
		return FALSE;

	if((host_properties & HOST_IS_NOT_FLAPPING) && temp_hoststatus->is_flapping==TRUE)
		return FALSE;

	if((host_properties & HOST_NOTIFICATIONS_DISABLED) && temp_hoststatus->notifications_enabled==TRUE)
		return FALSE;

	if((host_properties & HOST_NOTIFICATIONS_ENABLED) && temp_hoststatus->notifications_enabled==FALSE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECKS_DISABLED) && temp_hoststatus->accept_passive_host_checks==TRUE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECKS_ENABLED) && temp_hoststatus->accept_passive_host_checks==FALSE)
		return FALSE;

	if((host_properties & HOST_PASSIVE_CHECK) && temp_hoststatus->check_type==HOST_CHECK_ACTIVE)
		return FALSE;

	if((host_properties & HOST_ACTIVE_CHECK) && temp_hoststatus->check_type==HOST_CHECK_PASSIVE)
		return FALSE;

	if((host_properties & HOST_HARD_STATE) && temp_hoststatus->state_type==SOFT_STATE)
		return FALSE;

	if((host_properties & HOST_SOFT_STATE) && temp_hoststatus->state_type==HARD_STATE)
		return FALSE;

	return TRUE;
        }



/* check service properties filter */
int passes_service_properties_filter(servicestatus *temp_servicestatus){

	if((service_properties & SERVICE_SCHEDULED_DOWNTIME) && temp_servicestatus->scheduled_downtime_depth<=0)
		return FALSE;

	if((service_properties & SERVICE_NO_SCHEDULED_DOWNTIME) && temp_servicestatus->scheduled_downtime_depth>0)
		return FALSE;

	if((service_properties & SERVICE_STATE_ACKNOWLEDGED) && temp_servicestatus->problem_has_been_acknowledged==FALSE)
		return FALSE;

	if((service_properties & SERVICE_STATE_UNACKNOWLEDGED) && temp_servicestatus->problem_has_been_acknowledged==TRUE)
		return FALSE;

	if((service_properties & SERVICE_CHECKS_DISABLED) && temp_servicestatus->checks_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_CHECKS_ENABLED) && temp_servicestatus->checks_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_EVENT_HANDLER_DISABLED) && temp_servicestatus->event_handler_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_EVENT_HANDLER_ENABLED) && temp_servicestatus->event_handler_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_FLAP_DETECTION_DISABLED) && temp_servicestatus->flap_detection_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_FLAP_DETECTION_ENABLED) && temp_servicestatus->flap_detection_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_IS_FLAPPING) && temp_servicestatus->is_flapping==FALSE)
		return FALSE;

	if((service_properties & SERVICE_IS_NOT_FLAPPING) && temp_servicestatus->is_flapping==TRUE)
		return FALSE;

	if((service_properties & SERVICE_NOTIFICATIONS_DISABLED) && temp_servicestatus->notifications_enabled==TRUE)
		return FALSE;

	if((service_properties & SERVICE_NOTIFICATIONS_ENABLED) && temp_servicestatus->notifications_enabled==FALSE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECKS_DISABLED) && temp_servicestatus->accept_passive_service_checks==TRUE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECKS_ENABLED) && temp_servicestatus->accept_passive_service_checks==FALSE)
		return FALSE;

	if((service_properties & SERVICE_PASSIVE_CHECK) && temp_servicestatus->check_type==SERVICE_CHECK_ACTIVE)
		return FALSE;

	if((service_properties & SERVICE_ACTIVE_CHECK) && temp_servicestatus->check_type==SERVICE_CHECK_PASSIVE)
		return FALSE;

	if((service_properties & SERVICE_HARD_STATE) && temp_servicestatus->state_type==SOFT_STATE)
		return FALSE;

	if((service_properties & SERVICE_SOFT_STATE) && temp_servicestatus->state_type==HARD_STATE)
		return FALSE;

	return TRUE;
        }


