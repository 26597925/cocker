#include "vhm_in.h"

/* for test
$ vhm -a start --vhost test
*/

/* for debug
$ mount -t overlay overlay -o lowerdir=/var/openvh/vtemplate/test/rlayer,upperdir=/var/openvh/vhost/test/rwlayer,workdir=/var/openvh/vhost/test/workdir /var/openvh/vhost/test/merged
$ chroot /var/openvh/vhost/test/merged
$ mount -t proc proc /proc

$ umount /proc
$ umount /var/openvh/vhost/test/merged
*/

static unsigned char	stack_bottom[ 1024 * 1024 ] = {0} ;

static int VHostEntry( void *p )
{
	struct VhmEnvironment	*vhm_env = (struct VhmEnvironment *)p ;
	
	int			len ;
	
	char			netns_path[ PATH_MAX ] ;
	int			netns_fd ;
	
	char			hostname[ HOST_NAME_MAX ] ;
	char			vtemplate[ PATH_MAX ] ;
	char			mount_target[ PATH_MAX ] ;
	char			mount_data[ 4096 ] ;
	
	int			nret = 0 ;
	
	/* setns */
	memset( netns_path , 0x00 , sizeof(netns_path) );
	len = snprintf( netns_path , sizeof(netns_path)-1 , "/var/run/netns/netns-%s" , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(netns_path)-1) )
	{
		printf( "*** ERROR : netns path overflow\n" );
		return -1;
	}
	netns_fd = open( netns_path , O_RDONLY ) ;
	if( netns_fd == -1 )
	{
		printf( "*** ERROR : open netns path failed , errno[%d]\n" , errno );
		return -1;
	}
	
	nret = setns( netns_fd , CLONE_NEWNET ) ;
	if( nret == -1 )
	{
		printf( "*** ERROR : setns failed , errno[%d]\n" , errno );
		return -1;
	}
	
	close( netns_fd );
	
	/* basic seting */
	setuid(0);
	setgid(0);
	setgroups(0,NULL);
	
	/* setlocale */
	setlocale( LC_ALL , "C" );
	
	/* sethostname */
	nret = ReadFileLine( hostname , sizeof(hostname)-1 , NULL , -1 , "%s/%s/hostname" , vhm_env->vhosts_path_base , vhm_env->cmd_para.__vhost ) ;
	if( nret )
	{
		printf( "*** ERROR : ReadFileLine hostname in vhost '%s' failed\n" , vhm_env->cmd_para.__vhost );
		return -1;
	}
	sethostname( hostname , strlen(hostname) );
	
	unshare( CLONE_NEWUSER );
	
	/* mount filesystem */
	nret = ReadFileLine( vtemplate , sizeof(vtemplate)-1 , NULL , -1 , "%s/%s/vtemplates" , vhm_env->vhosts_path_base , vhm_env->cmd_para.__vhost ) ;
	if( nret )
	{
		printf( "*** ERROR : ReadFileLine vtemplates in vhost '%s' failed\n" , vhm_env->cmd_para.__vhost );
		return -1;
	}
	
	memset( mount_target , 0x00 , sizeof(mount_target) );
	len = snprintf( mount_target , sizeof(mount_target)-1 , "%s/%s/merged" , vhm_env->vhosts_path_base , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(mount_target)-1) )
	{
		printf( "*** ERROR : snprintf failed\n" );
		return -1;
	}
	
	memset( mount_data , 0x00 , sizeof(mount_data) );
	if( vtemplate[0] == '\0' )
		len = snprintf( mount_data , sizeof(mount_data)-1 , "upperdir=%s/%s/rwlayer,workdir=%s/%s/workdir" , vhm_env->vhosts_path_base,vhm_env->cmd_para.__vhost , vhm_env->vhosts_path_base,vhm_env->cmd_para.__vhost ) ;
	else
		len = snprintf( mount_data , sizeof(mount_data)-1 , "lowerdir=%s/%s/rlayer,upperdir=%s/%s/rwlayer,workdir=%s/%s/workdir" , vhm_env->vtemplates_path_base,vtemplate , vhm_env->vhosts_path_base,vhm_env->cmd_para.__vhost , vhm_env->vhosts_path_base,vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(mount_data)-1) )
	{
		printf( "*** ERROR : snprintf failed\n" );
		return -1;
	}
	
	nret = mount( "overlay" , mount_target , "overlay" , MS_MGC_VAL , (void*)mount_data ) ;
	if( nret == -1 )
	{
		printf( "*** ERROR : mount[%s][%s] failed , errno[%d]\n" , mount_data , mount_target , errno );
		return -1;
	}
	
	/* chroot */
	chroot( mount_target );
	chdir( "/root" );
	
	/* mount /proc */
	mount( "proc" , "/proc" , "proc" , MS_MGC_VAL , NULL );
	
	/* execl */
	nret = execl( "/bin/bash" , "bash" , "--login" , NULL ) ;
	if( nret == -1 )
	{
		printf( "*** ERROR : execl failed , errno[%d]\n" , errno );
		return -1;
	}
	
	return 0;
}

int VhmAction_start( struct VhmEnvironment *vhm_env )
{
	char		cmd[ 4096 ] ;
	int		len ;
	
	char		netns_name[ ETHERNET_NAME_MAX + 1 ] ;
	char		veth1_name[ ETHERNET_NAME_MAX + 1 ] ;
	char		veth0_name[ ETHERNET_NAME_MAX + 1 ] ;
	char		veth0_sname[ ETHERNET_NAME_MAX + 1 ] ;
	char		vnetbr_name[ ETHERNET_NAME_MAX + 1 ] ;
	
	pid_t		pid ;
	char		pid_str[ 20 + 1 ] ;
#if 0
	int		status ;
#endif
	int		nret = 0 ;
	
	/* up network */
	memset( netns_name , 0x00 , sizeof(netns_name) );
	len = snprintf( netns_name , sizeof(netns_name)-1 , "netns-%s" , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(netns_name)-1) )
	{
		printf( "*** ERROR : netns name overflow\n" );
		return -1;
	}
	
	memset( veth1_name , 0x00 , sizeof(veth1_name) );
	len = snprintf( veth1_name , sizeof(veth1_name)-1 , "veth1-%s" , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(veth1_name)-1) )
	{
		printf( "*** ERROR : veth1 name overflow\n" );
		return -1;
	}
	
	memset( veth0_name , 0x00 , sizeof(veth0_name) );
	len = snprintf( veth0_name , sizeof(veth0_name)-1 , "veth0-%s" , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(veth0_name)-1) )
	{
		printf( "*** ERROR : veth0 name overflow\n" );
		return -1;
	}
	
	memset( veth0_sname , 0x00 , sizeof(veth0_sname) );
	len = snprintf( veth0_sname , sizeof(veth0_sname)-1 , "eth0" ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(veth0_sname)-1) )
	{
		printf( "*** ERROR : veth0 sname overflow\n" );
		return -1;
	}
	
	memset( vnetbr_name , 0x00 , sizeof(vnetbr_name) );
	len = snprintf( vnetbr_name , sizeof(vnetbr_name)-1 , "vnetbr-%s" , vhm_env->cmd_para.__vhost ) ;
	if( SNPRINTF_OVERFLOW(len,sizeof(vnetbr_name)-1) )
	{
		printf( "*** ERROR : vnetbr name overflow\n" );
		return -1;
	}
	
	nret = SnprintfAndSystem( cmd , sizeof(cmd) , "ifconfig %s up" , vnetbr_name ) ;
	if( nret )
	{
		printf( "*** ERROR : [%s] failed[%d] , errno[%d]\n" , cmd , nret , errno );
		return -1;
	}
	
	nret = SnprintfAndSystem( cmd , sizeof(cmd) , "ifconfig %s up" , veth1_name ) ;
	if( nret )
	{
		printf( "*** ERROR : [%s] failed [%d], errno[%d]\n" , cmd , nret , errno );
		return -1;
	}
	
	nret = SnprintfAndSystem( cmd , sizeof(cmd) , "ip netns exec %s ifconfig %s up" , netns_name , veth0_sname ) ;
	if( nret )
	{
		printf( "*** ERROR : [%s] failed[%d] , errno[%d]\n" , cmd , nret , errno );
		return -1;
	}
	
	nret = SnprintfAndSystem( cmd , sizeof(cmd) , "ip netns exec %s ifconfig lo up" , netns_name ) ;
	if( nret )
	{
		printf( "*** ERROR : [%s] failed[%d] , errno[%d]\n" , cmd , nret , errno );
		return -1;
	}
	
	nret = SnprintfAndSystem( cmd , sizeof(cmd) , "ip netns exec %s route add default gw 192.168.8.1 netmask 0.0.0.0 dev %s" , netns_name , veth0_sname ) ;
	if( nret )
	{
		printf( "*** ERROR : [%s] failed[%d] , errno[%d]\n" , cmd , nret , errno );
		return -1;
	}
	
	/* create vhost */
	pid = clone( VHostEntry , stack_bottom+sizeof(stack_bottom) , CLONE_NEWNS|CLONE_NEWPID|CLONE_NEWIPC|CLONE_NEWUTS|CLONE_NEWNET , (void*)vhm_env ) ;
	if( pid == -1 )
	{
		printf( "*** ERROR : clone failed[%d] , errno[%d]\n" , pid , errno );
		return -1;
	}
	
	/* write pid file */
	memset( pid_str , 0x00 , sizeof(pid_str) );
	snprintf( pid_str , sizeof(pid_str)-1 , "%d" , pid );
	nret = WriteFileLine( pid_str , NULL , -1 , "%s/%s/pid" , vhm_env->vhosts_path_base , vhm_env->cmd_para.__vhost ) ;
	if( nret )
	{
		printf( "*** ERROR : WriteFileLine failed[%d] , errno[%d]\n" , nret , errno );
		return -1;
	}
	
	/* wait for vhost end */
#if 0
	while(1)
	{
		pid = waitpid( -1 , NULL , 0 ) ;
		if( pid == -1 )
		{
			if( errno == ECHILD )
				break;
			
			printf( "*** ERROR : waitpid failed , errno[%d]\n" , errno ); fflush(stdout);
			return -1;
		}
	}
#endif
	while( wait(NULL) < 0 )
	{
		continue;
	}
	
	/* kill clone process */
	kill( pid , SIGKILL );
	
	/* cleanup pid file */
	SnprintfAndUnlink( NULL , -1 , "%s/%s/pid" , vhm_env->vhosts_path_base , vhm_env->cmd_para.__vhost );
	
	printf( "OK\n" );
	
	return 0;
}
