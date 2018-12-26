#include "confd_shm.h"
#include "log.h"

static bool confd_shm_alloc(confd_shm_t *shm);
static bool confd_shm_free(confd_shm_t *shm);

confd_shm_t*
init_shm(size_t shm_size)
{
    confd_shm_t* shm = (confd_shm_t*)malloc(sizeof(confd_shm_t));
    shm->size = shm_size;

    if (!confd_shm_alloc(shm)) {
        BOOST_LOG_TRIVIAL(warning) << "alloc shm size(" << shm_size << ") failed.";
        return NULL;
    }
    return shm;
}

bool
destory_shm(confd_shm_t *shm)
{
    if (!confd_shm_free(shm)) {                   //释放共享内存
        BOOST_LOG_TRIVIAL(info) << "shm free failed.";
        return false;
    }
    free(shm);
    return true; 
}

#if (USE_MMAP_ANON)

bool
confd_shm_alloc(confd_shm_t *shm)
{
    shm->addr = (char *) mmap(NULL, shm->size,
                                PROT_READ|PROT_WRITE,
                                MAP_ANON|MAP_SHARED, -1, 0);

    if (shm->addr == MAP_FAILED) {
        BOOST_LOG_TRIVIAL(error) << "mmap ipc_name(/dev/zero) failed, error: " << strerror(errno);
        return false;
    }

    return true;
}


bool
confd_shm_free(confd_shm_t *shm)
{
    char buf[1024];

    if (munmap((void *) shm->addr, shm->size) == -1) {
        sprintf(buf, "munmap(%p, %ld) failed", shm->addr, shm->size);
        BOOST_LOG_TRIVIAL(warning) << std::string(buf);
        return false;
    }
    return true;
}


#elif (USE_MMAP_DEVZERO)
bool
confd_shm_alloc(confd_shm_t *shm)
{
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if (fd < 0) {
        BOOST_LOG_TRIVIAL(error) << "open ipc_name(/dev/zero) failed, error: " << strerror(errno);
        return false;
    }
    shm->addr = (char *) mmap(NULL, shm->size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm->addr == MAP_FAILED) {
        BOOST_LOG_TRIVIAL(error) << "mmap ipc_name(/dev/zero) failed, error: " << strerror(errno);
        return false;
    }
    if (close(fd) == -1) {
        BOOST_LOG_TRIVIAL(warning) << "close ipc_name(/dev/zero) failed, error: " << strerror(errno);
    }
    return true;
}

bool
confd_shm_free(confd_shm_t *shm)
{
    char buf[1024];

    if (munmap((void *) shm->addr, shm->size) == -1) {
        sprintf(buf, "munmap(%p, %ld) failed", shm->addr, shm->size);
        BOOST_LOG_TRIVIAL(warning) << std::string(buf);
        return false;
    }
    return true;
}

#elif (USE_SHM)

#include <sys/ipc.h>
#include <sys/shm.h>

bool
confd_shm_alloc(confd_shm_t *shm)
{
    int  id;

    id = shmget(IPC_PRIVATE, shm->size, (SHM_R|SHM_W|IPC_CREAT));

    if (id == -1) {
        BOOST_LOG_TRIVIAL(warning) << "shmget(" << shm->size << ") failed.";
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "shmget id=" << id ;

    shm->addr = (char*) shmat(id, NULL, 0);

    if (shm->addr == (void *) -1) {
        BOOST_LOG_TRIVIAL(warning) << "shmat() failed.";
    }

    if (shmctl(id, IPC_RMID, NULL) == -1) {
        BOOST_LOG_TRIVIAL(warning) << "shmctl(IPC_RMID) failed.";
    }

    return (shm->addr == (void *) -1) ? false : true;
}


bool
confd_shm_free(confd_shm_t *shm)
{
    char buf[1024];

    if (shmdt(shm->addr) == -1) {
        sprintf(buf, "munmap(%p, %ld) failed", shm->addr, shm->size);
        BOOST_LOG_TRIVIAL(warning) << std::string(buf);
        return false;
    }
    return true;
}

#endif
