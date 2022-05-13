#include <stdlib.h>
#include <systemd/sd-bus.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <poll.h>

#define USEC_PER_SEC ((uint64_t)1000000ULL)
#define USEC_PER_MSEC ((uint64_t)1000ULL)

enum verify_return_state {
  STATE_INCOMPLETE,
  STATE_ERROR
};

typedef struct verify_data {
  char *device_path;
  char *result;
  bool verify_started;
  int verify_state;
} verify_data;

typedef int fd_int;

static void fd_cleanup(int *fd) {
  if (*fd >= 0)
    close(*fd);
}

static bool str_equal(const char *a, const char *b) {
  if (a == NULL && b == NULL)
    return true;
  if (a == NULL || b == NULL)
    return false;
  return strcmp(a, b) == 0;
}

static char *get_default_device(sd_bus *bus) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *message = NULL;
  int result;
  const char *device_path = NULL;

  if (sd_bus_call_method(bus, "net.reactivated.Fprint", 
                         "/net/reactivated/Fprint/Manager",
                         "net.reactivated.Fprint.Manager",
                         "GetDefaultDevice",
                         &error,
                         &message,
                         NULL) < 0) {
    fprintf(stderr, "GetDefaultDevice Failed: %s\n", error.message);
    return NULL;
  }

  sd_bus_message_read_basic(message, SD_BUS_TYPE_OBJECT_PATH, &device_path);

  return device_path ? strdup(device_path) : NULL;
}

static bool claim_device(sd_bus *bus, const char *device_path) {
  sd_bus_error error = SD_BUS_ERROR_NULL;

  if(sd_bus_call_method(bus, 
                        "net.reactivated.Fprint",
                        device_path,
                        "net.reactivated.Fprint.Device",
                        "Claim",
                        &error,
                        NULL,
                        "s",
                        "") < 0) {
    fprintf(stderr, "Unable to claim device %s: %s\n", device_path, error.message);
    return false;
  }

  return true;
}

static void release_device(sd_bus *bus, const char *device_path) {
  sd_bus_error error = SD_BUS_ERROR_NULL;

  if(sd_bus_call_method(bus,
                        "net.reactivated.Fprint",
                        device_path,
                        "net.reactivated.Fprint.Device",
                        "Release",
                        &error,
                        NULL,
                        NULL,
                        NULL) < 0) {
    fprintf(stderr, "Unable to release device %s: %s", device_path, error.message);
  }
}

static int verify_result_cb(sd_bus_message *bus_message, void *userdata, sd_bus_error *return_error) {
  verify_data *data = userdata;
  const char *result;
  uint64_t done = false;
  int return_val;

  if (!sd_bus_message_is_signal(bus_message, "net.reactivated.Fprint.Device", "VerifyStatus")) {
    // Not the signalwe expected
    return 0;
  }

  if ((return_val = sd_bus_message_read(bus_message, "sb", &result, &done)) < 0) {
    // Failed to parse VerifyResult signal
    data->verify_state = STATE_ERROR;
    return 0;
  }

  if (!data->verify_started) {
    // Unexpected VerifyResult; verify has not been started
    return 0;
  }

  if (done && result) {
    data->result = strdup(result);
    return 0;
  }

  // Error with scanning

  return 0;
}

static int verify_started_cb(sd_bus_message *bus_message, void *userdata, sd_bus_error *return_error) {
  verify_data *data = userdata;
  const sd_bus_error *error = sd_bus_message_get_error(bus_message);

  if(error) {
    if (sd_bus_error_has_name(error, "net.reactivated.Fprint.Error.NoEnrolledPrints")) {
      fprintf(stderr, "No fingerprints enrolled\n");
      data->verify_state = STATE_ERROR;
    }

    return 1;
  }

  data->verify_started = true;

  return 1;
}

static bool verify_fingerprint(sd_bus *bus, verify_data *data) {
  sd_bus_slot *verify_status_slot = NULL;
  sigset_t signals;
  fd_int signal_fd = -1;
  int result;

  sd_bus_match_signal(bus,
                    &verify_status_slot,
                    "net.reactivated.Fprint",
                    data->device_path,
                    "net.reactivated.Fprint.Device",
                    "VerifyStatus",
                    verify_result_cb,
                    data);

  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  signal_fd = signalfd(signal_fd, &signals, SFD_NONBLOCK);

  data->verify_started = false;
  data->verify_state = STATE_INCOMPLETE;
  data->result = NULL;
  result = sd_bus_call_method_async(bus,
                                    NULL,
                                    "net.reactivated.Fprint",
                                    data->device_path,
                                    "net.reactivated.Fprint.Device",
                                    "VerifyStart",
                                    verify_started_cb,
                                    data,
                                    "s",
                                    "any");
  
  if(result < 0) {
    fprintf(stderr, "VerifyStart call failed: %d\n", result);
    return false;
  }

  for(;;) {
    struct signalfd_siginfo siginfo;

    if(read(signal_fd, &siginfo, sizeof(siginfo)) > 0) {
      fprintf(stderr, "Received signal %d during verify", siginfo.ssi_signo);

      // Should only happen when SIGINT is received
      return false;
    }

    result = sd_bus_process(bus, NULL);
    if(result < 0) {
      break;
    }
    if(data->verify_state != STATE_INCOMPLETE)
      break;
    if (!data->verify_started) {
      continue;
    }
    if (data->result != NULL) {
      break;
    }

    if (result == 0) {
      struct pollfd fds[2] = {
        {sd_bus_get_fd(bus), sd_bus_get_events(bus), 0},
        {signal_fd, POLLIN, 0},
      };

      result = poll(fds, 2, (30 * USEC_PER_SEC) / USEC_PER_MSEC);
      if (result < 0 && errno != EINTR) {
        fprintf(stderr, "Error waiting for events: %d\n", errno);
        return false;
      }
    }
  }

  sd_bus_call_method(bus,
                     "net.reactivated.Fprint",
                     data->device_path,
                     "net.reactivated.Fprint.Device",
                     "VerifyStop",
                     NULL,
                     NULL,
                     NULL,
                     NULL);

  fd_cleanup(&signal_fd);

  if (data->verify_state != STATE_INCOMPLETE) {
    return false;
  }

  if(str_equal(data->result, "verify-match")) {
    // Match
    return true;
  }

  return false;
}

int main(int argc, char *argv[]) {
  sd_bus* bus = NULL;
  char *device = NULL;
  verify_data *data = calloc(1, sizeof(verify_data));

  if(!data) {
    fprintf(stderr, "Unable to allocate sufficient memory\n");
    return EXIT_FAILURE;
  }

  if(sd_bus_open_system(&bus) < 0) {
    fprintf(stderr, "Unable to open system bus: %d\n", errno);
    free(data);
    return EXIT_FAILURE;
  }

  data->device_path = get_default_device(bus);

  if(!data->device_path) {
    fprintf(stderr, "Unable to fetch default device path\n");
    sd_bus_close(bus);
    free(data);
    return EXIT_FAILURE;
  }

  if(!claim_device(bus, data->device_path)) {
    fprintf(stderr, "Unable to claim device\n");
    return EXIT_FAILURE;
  }

  if(verify_fingerprint(bus, data)) {
    printf("Fingerprint matched!\n");
  } else {
    printf("Fingerprint did not match\n");
  }

  release_device(bus, data->device_path);
  sd_bus_close(bus);

  free(data->device_path);
  if(data->result) {
    free(data->result);
  }
  free(data);

  return EXIT_SUCCESS;
}