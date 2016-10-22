#include <stdio.h>
#include <pthread.h>
#include <curl/curl.h>


#if defined(USE_OPENSSL)
#include <openssl/crypto.h>

static pthread_mutex_t *dlssl_locks;

static void dlssl_lock_callback(int mode, int type, char *file, int line)
{
	(void)file;
	(void)line;

	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&dlssl_locks[type]);
	else
		pthread_mutex_unlock(&dlssl_locks[type]);
}

static unsigned long dlssl_thread_id(void)
{
	return (unsigned long)pthread_self();
}

void dlssl_locks_init(void)
{
	int i;

	if (!(dlssl_locks = OPENSSL_malloc(CRYPTO_num_locks() *
					    sizeof(dlssl_locks[0]))))
		return;

	for (i = 0; i < CRYPTO_num_locks(); i++) {
		pthread_mutex_init(&dlssl_locks[i], NULL);
	}

	CRYPTO_set_id_callback(dlssl_thread_id);
	CRYPTO_set_locking_callback(dlssl_lock_callback);
}

void dlssl_locks_destroy(void)
{
	int i;

	CRYPTO_set_locking_callback(NULL);
	for (i = 0; i < CRYPTO_num_locks(); i++) {
		pthread_mutex_destroy(&dlssl_locks[i]);
	}
	OPENSSL_free(dlssl_locks);
}

#elif defined(USE_GNUTLS)
#include <gcrypt.h>
#include <errno.h>

GCRY_THREAD_OPTION_PTHREAD_IMPL;

void dlssl_locks_init(void)
{
	gcry_control(GCRYCTL_SET_THREAD_CBS);
}

void dlssl_locks_destroy(void)
{
	return;
}

#else
void dlssl_locks_init(void)
{
	return;
}

void dlssl_locks_destroy(void)
{
	return;
}

#endif
