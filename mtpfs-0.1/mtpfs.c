/*
FUSE: Filesystem in Userspace

See the file COPYING.
*/
#include "mtpfs.h"

#if DEBUG
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define DBG(a...) {g_printf( "[" __FILE__ ":" TOSTRING(__LINE__) "] " a );g_printf("\n");}
#else
#define DBG(a...)
#endif

#if DEBUG

static void dump_mtp_error()
{
	LIBMTP_Dump_Errorstack(device);
	LIBMTP_Clear_Errorstack(device);
}
#else
#define dump_mtp_error()
#endif

#define enter_lock(a...)       do { DBG("lock"); DBG(a); g_static_mutex_lock(&device_lock); } while(0)
#define return_unlock(a)       do { DBG("return unlock"); g_static_mutex_unlock(&device_lock); return a; } while(0)

static GStaticMutex device_lock = G_STATIC_MUTEX_INIT;

struct _folder_list {
	LIBMTP_folder_t *folder;
	struct _folder_list *next;
};
struct mtp_filesystem {
	LIBMTP_mtpdevice_t *device;
	LIBMTP_folder_t *folders;
	LIBMTP_file_t *files;
	struct _folder_list *folder_list;
};
struct mtp_filesystem my_mtpfs;

static struct usb_bus* init_usb();
static uint32_t isliving(uint32_t bus_location,uint8_t dev_num,uint16_t vendor_id,uint16_t product_id);

static uint32_t lookup_folder_id_ex(LIBMTP_folder_t * folder, const char *path,
				    char *parent);
static int parse_path_ex(const char *path, LIBMTP_file_t * files,
			 LIBMTP_folder_t * folders);
static void tranverse_folder(struct _folder_list **folder_list,
			     LIBMTP_folder_t * folder);

static void tranverse_folder(struct _folder_list **folder_list,
			     LIBMTP_folder_t * folder)
{
	struct _folder_list *folder_list_t = NULL;
	if (folder != NULL) {
		folder_list_t = malloc(sizeof(struct _folder_list));
		folder_list_t->folder = folder;
		folder_list_t->next = *folder_list;
		*folder_list = folder_list_t;
		tranverse_folder(folder_list, folder->sibling);
		tranverse_folder(folder_list, folder->child);
	}
}

static int
mtpfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	      off_t offset, struct fuse_file_info *fi)
{
	(void)fi;
	(void)offset;
	LIBMTP_file_t *file_it;
	LIBMTP_folder_t *folder_it;
	struct _folder_list *folder_list_it;
	int parent_id;

	enter_lock("readdir %s", path);

	// Add common entries
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	parent_id = parse_path_ex(path, my_mtpfs.files, my_mtpfs.folders);
	DBG("file_it->parent_id=%d\n", parent_id);
	for (file_it = my_mtpfs.files; file_it != NULL; file_it = file_it->next) {
		if (file_it->parent_id == parent_id) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			//st.st_ino = file_it->item_id;
			st.st_mode = S_IFREG | 0444;
			st.st_nlink = 1;
			filler(buf,
			       (file_it->filename ==
				NULL ? "<mtpfs null>" : file_it->filename), &st,
			       0);
		}

	}
	for (folder_list_it = my_mtpfs.folder_list; folder_list_it != NULL;
	     folder_list_it = folder_list_it->next) {
		if (folder_list_it->folder->parent_id == parent_id) {
			struct stat st;
			memset(&st, 0, sizeof(st));
			//st.st_ino = folder_list_it->folder->folder_id;
			st.st_mode = S_IFDIR | 0755;
			st.st_nlink = 2;
			filler(buf, folder_list_it->folder->name, &st, 0);
		}
	}
	DBG("readdir exit");
	return_unlock(0);
}

static int mtpfs_getattr_real(const char *path, struct stat *stbuf)
{
	int ret = 0;
	int item_id = 0;
	gboolean found = FALSE;
	LIBMTP_file_t *file_it = NULL;
	LIBMTP_folder_t *folder_it = NULL;
	struct _folder_list *folder_list_it;
	struct fuse_context *fc;

	if (path == NULL)
		return -ENOENT;
	memset(stbuf, 0, sizeof(struct stat));

	// Set uid/gid of file
	fc = fuse_get_context();
	stbuf->st_uid = fc->uid;
	stbuf->st_gid = fc->gid;
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	DBG("**path=%s\n", path);
	item_id = parse_path_ex(path, my_mtpfs.files, my_mtpfs.folders);
	DBG("**item_id=%d\n", item_id);

	for (file_it = my_mtpfs.files; file_it != NULL; file_it = file_it->next) {
		if (file_it->item_id == item_id) {
			DBG("**file_it->filename=%s\n", file_it->filename);
			if (file_it->filetype == LIBMTP_FILETYPE_FOLDER) {
			} else {
				//stbuf->st_ino = item_id;
				stbuf->st_size = file_it->filesize;
				stbuf->st_blocks = (file_it->filesize / 512) +
				    (file_it->filesize % 512 > 0 ? 1 : 0);
				stbuf->st_nlink = 1;
				stbuf->st_mode = S_IFREG | 0444;
				DBG("time:%s",
				    ctime(&(file_it->modificationdate)));
				stbuf->st_mtime = file_it->modificationdate;
				stbuf->st_ctime = file_it->modificationdate;
				stbuf->st_atime = file_it->modificationdate;
				found = TRUE;
				return 0;
			}
		}
	}
	for (folder_list_it = my_mtpfs.folder_list; folder_list_it != NULL;
	     folder_list_it = folder_list_it->next) {
		if (folder_list_it->folder->folder_id == item_id) {
			//stbuf->st_ino = item_id;
			stbuf->st_mode = S_IFDIR | 0755;
			stbuf->st_nlink = 2;
			found = TRUE;
			return 0;
		}
	}
	if (!found) {
		stbuf->st_mode = S_IFIFO | 0755;
		stbuf->st_nlink = 1;
		//ret = -ENOENT;
	}
	return 0;
}

static int mtpfs_getattr(const char *path, struct stat *stbuf)
{
	enter_lock("getattr %s", path);

	int ret = mtpfs_getattr_real(path, stbuf);

	DBG("getattr exit");
	return_unlock(ret);
}

static int mtpfs_open(const char *path, struct fuse_file_info *fi)
{
	enter_lock("open");

	return_unlock(0);
}

static int
mtpfs_read(const char *path, char *buf, size_t size, off_t offset,
	   struct fuse_file_info *fi)
{
	int ret;
	int len = 0;
	int item_id = -1;
	(void)fi;
	size_t maxbytes = size;
	unsigned char *buffer = NULL;

	enter_lock("read");

	DBG("path=%s\nbuf=%d,size=%d,offset=%d\n", path, buf, size, offset);

	item_id = parse_path_ex(path, my_mtpfs.files, my_mtpfs.folders);
	DBG("item_id=%d\n", item_id);
	if (item_id < 0)
		return_unlock(-ENOENT);

	LIBMTP_Get_Partialobject_To_Buffer(my_mtpfs.device, item_id, offset,
					   maxbytes, &buffer, &len, NULL, NULL);

	DBG("readlen=%d", len);
	ret = len;
	memcpy(buf, buffer, len);
	free(buffer);
	buffer = NULL;

	if (ret == -1)
		ret = -errno;
	return_unlock(ret);
}

int mtpfs_blank()
{
	// Do nothing
}

static struct fuse_operations mtpfs_oper = {
	.readdir = mtpfs_readdir,
	.getattr = mtpfs_getattr,
	.open = mtpfs_open,
	.read = mtpfs_read,
};

int main(int argc, char *argv[])
{

	int fuse_stat;
	umask(0);
	LIBMTP_raw_device_t *rawdevices = NULL;
	LIBMTP_mtpdevice_t *device = NULL;
	int numrawdevices = 0;
	LIBMTP_error_number_t err;
	int i = 0;
	int have_busnum = 0;
	int have_devnum = 0;
	uint32_t busnum = 0;
	uint8_t devnum = 0;
	uint32_t opened_bus_location = 0;
	uint8_t opened_devnum = 0;
	uint16_t opened_vendor_id = 0;
	uint16_t opened_product_id = 0;

	int opt;
	extern int optind;
	extern char *optarg;
	char **argv_new = NULL;
	int *argc_new = 0;
	int argv_i = 0;
	int argv_new_i = 0;

	//while ((opt = getopt(argc, argv, "d")) != -1 ) {
	//switch (opt) {
	//case 'd':
	////LIBMTP_Set_Debug(9);
	//break;
	//}
	//}

	//argc -= optind;
	//argv += optind;
	

	//find devnum and busnum in args
	printf("argc=%d\n",argc);
	if(argc<2){
		//usage();
		return 1;
	}else{
		argv_new=malloc(argc*sizeof(char *));
	}
	for(argv_i=0,argv_new_i=0;argv_i<argc;argv_i++){
		if(strcasecmp(argv[argv_i],"-bus")==0){
			argv_i++;
			busnum=strtol(argv[argv_i],NULL,10);
			have_busnum=1;
			continue;
		}else if(strcasecmp(argv[argv_i],"-dev")==0){
			argv_i++;
			devnum=strtol(argv[argv_i],NULL,10);
			have_devnum=1;
			continue;
		}
		argv_new[argv_new_i]=argv[argv_i];
		argv_new_i++;
	}
	argc_new=argv_new_i;

	LIBMTP_Init();

	fprintf(stdout, "Listing raw device(s)\n");
	err = LIBMTP_Detect_Raw_Devices(&rawdevices, &numrawdevices);
	switch (err) {
	case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
		fprintf(stdout, "   No raw devices found.\n");
		return 0;
	case LIBMTP_ERROR_CONNECTING:
		fprintf(stderr,
			"Detect: There has been an error connecting. Exiting\n");
		return 1;
	case LIBMTP_ERROR_MEMORY_ALLOCATION:
		fprintf(stderr,
			"Detect: Encountered a Memory Allocation Error. Exiting\n");
		return 1;
	case LIBMTP_ERROR_NONE:
		{
			int i;

			fprintf(stdout, "   Found %d device(s):\n",
				numrawdevices);
			for (i = 0; i < numrawdevices; i++) {
				if (rawdevices[i].device_entry.vendor != NULL ||
				    rawdevices[i].device_entry.product !=
				    NULL) {
					fprintf(stdout,
						"   %s: %s (%04x:%04x) @ bus %d, dev %d\n",
						rawdevices[i].device_entry.
						vendor,
						rawdevices[i].device_entry.
						product,
						rawdevices[i].device_entry.
						vendor_id,
						rawdevices[i].device_entry.
						product_id,
						rawdevices[i].bus_location,
						rawdevices[i].devnum);
				} else {
					fprintf(stdout,
						"   %04x:%04x @ bus %d, dev %d\n",
						rawdevices[i].device_entry.
						vendor_id,
						rawdevices[i].device_entry.
						product_id,
						rawdevices[i].bus_location,
						rawdevices[i].devnum);
				}
			}
		}
		break;
	case LIBMTP_ERROR_GENERAL:
	default:
		fprintf(stderr, "Unknown connection error.\n");
		return 1;
	}

	i=0;
	if(have_devnum&&have_busnum){
		fprintf(stdout, "Attempting to connect device @ bus %d,dev %d\n",busnum,devnum);
		for (i = 0; i < numrawdevices; i++) {
			if (rawdevices[i].bus_location == busnum &&
			rawdevices[i].devnum==devnum){
				break;
			}
		}
	}else{
		fprintf(stdout, "Attempting to connect device\n");
	}
	opened_bus_location = rawdevices[i].bus_location;
	opened_devnum = rawdevices[i].devnum;
	opened_vendor_id = rawdevices[i].device_entry.vendor_id;
	opened_product_id = rawdevices[i].device_entry.product_id;
	device = LIBMTP_Open_Raw_Device(&rawdevices[i]);
	//device = LIBMTP_Open_Raw_Device_Uncached(&rawdevices[i]);
	if (device == NULL) {
		fprintf(stderr, "Unable to open raw device %d\n", i);
		return 1;
	}
	
	LIBMTP_Dump_Errorstack(device);
	LIBMTP_Clear_Errorstack(device);

	my_mtpfs.device = device;
	my_mtpfs.folders = LIBMTP_Get_Folder_List(device);
	my_mtpfs.files =
	    LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
	my_mtpfs.folder_list = NULL;
	tranverse_folder(&(my_mtpfs.folder_list), my_mtpfs.folders);
	DBG("my_mtpfs.folders%d\n", my_mtpfs.folders);
	DBG("my_mtpfs.files%d\n", my_mtpfs.files);
	DBG("my_mtpfs.folder_list%d\n", my_mtpfs.folder_list);

	struct _folder_list *folder_list_it = my_mtpfs.folder_list;
	for (folder_list_it = my_mtpfs.folder_list; folder_list_it != NULL;
	     folder_list_it = folder_list_it->next) {
		DBG("folder_list_it->folder->name=%s\n",
		    folder_list_it->folder->name);
	}
	//check whether the device that was opend was living.
	if(0 == isliving(opened_bus_location,
		       		opened_devnum,
				opened_vendor_id,
				opened_product_id)){
		exit(1);
	}else{
		printf("%s\n","Start fuse");	
	}
	fuse_stat = fuse_main(argc_new, argv_new, &mtpfs_oper);
	if(argv_new != NULL){
		free(argv_new);
	}
	DBG("fuse_main returned %d\n", fuse_stat);
	return fuse_stat;
}
static uint32_t
isliving(uint32_t bus_location,uint8_t devnum,uint16_t vendor_id,uint16_t product_id){
	int found = 0;
	struct usb_bus *bus = init_usb();
	for (; bus != NULL; bus = bus->next) {
		if(bus->location == bus_location){ //check bus_location
			struct usb_device *dev = bus->devices;
			for (; dev != NULL; dev = dev->next) {
				if (dev->descriptor.bDeviceClass != USB_CLASS_HUB) {
					// First check if we know about the device already.
					// Devices well known to us will not have their descriptors
					// probed, it caused problems with some devices.
					if(dev->devnum == devnum &&
							dev->descriptor.idVendor == vendor_id &&
							dev->descriptor.idProduct == product_id) {
						found = 1;
						return found;
					}
				}	
			}
		}
	}
	if(found == 0){
		printf("the device at %.4x:%.4x @ bus %x,dev %x is dead.\n",vendor_id,product_id,bus_location,devnum);
	}
	return found;
}

static struct usb_bus* init_usb()
{
  struct usb_bus* busses;
  struct usb_bus* bus;

  usb_init();
  usb_find_busses();
  usb_find_devices();
  /* Workaround a libusb 0.1 bug : bus location is not initialised */
  busses = usb_get_busses();
  for (bus = busses; bus != NULL; bus = bus->next) {
      bus->location = strtoul(bus->dirname, NULL, 10);
  }
  return (busses);
}

/* Find the folder_id of a given path
 * Runs by walking through folders structure */
static uint32_t
lookup_folder_id_ex(LIBMTP_folder_t * folder, const char *path, char *parent)
{
	char *current;
	uint32_t ret = (uint32_t) - 1;

	if (strcmp(path, "/") == 0)
		return 0;

	if (folder == NULL) {
		return ret;
	}

	current = malloc(strlen(parent) + strlen(folder->name) + 2);
	sprintf(current, "%s/%s", parent, folder->name);
	if (strcasecmp(path, current) == 0) {
		free(current);
		return folder->folder_id;
	}
	if (strncasecmp(path, current, strlen(current)) == 0) {
		ret = lookup_folder_id_ex(folder->child, path, current);
	}
	free(current);
	if (ret != (uint32_t) (-1)) {
		return ret;
	}
	ret = lookup_folder_id_ex(folder->sibling, path, parent);
	return ret;
}

/* Parses a string to find item_id */
static int
parse_path_ex(const char *path, LIBMTP_file_t * files,
	      LIBMTP_folder_t * folders)
{
	char *rest;
	uint32_t item_id;

	// Check if path is an item_id
	if (*path != '/') {
		item_id = strtoul(path, &rest, 0);
		// really should check contents of "rest" here...
		/* if not number, assume a file name */
		if (item_id == 0) {
			LIBMTP_file_t *file = files;

			/* search for matching name */
			while (file != NULL) {
				if (strcasecmp(file->filename, path) == 0) {
					return file->item_id;
				}
				file = file->next;
			}
		}
		return item_id;
	}
	// Check if path is a folder
	//DBG("*************path%s\n",path);
	item_id = lookup_folder_id_ex(folders, path, "");
	if (item_id == (uint32_t) - 1) {
		char *dirc = strdup(path);
		char *basec = strdup(path);
		char *parent = dirname(dirc);
		char *filename = basename(basec);
		uint32_t parent_id = lookup_folder_id_ex(folders, parent, "");
		LIBMTP_file_t *file;

		file = files;
		while (file != NULL) {
			if (file->parent_id == parent_id) {
				if (strcasecmp(file->filename, filename) == 0) {
					free(dirc);
					free(basec);
					return file->item_id;
				}
			}
			file = file->next;
		}
		free(dirc);
		free(basec);
	} else {
		return item_id;
	}

	return -1;
}
