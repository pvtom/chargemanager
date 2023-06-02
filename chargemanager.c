#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#if !defined(__MACH__)
#include <malloc.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <regex.h>
#include <signal.h>
#include <mosquitto.h>

#define INTERVAL_DEFAULT  300
#define INTERVAL_FAST     10

#define REDUCED_CHARGE_POWER      5500
#define BATTERY_MAX_POWER_DEFAULT 4500
#define HYSTERESIS_MIN_DEFAULT    85
#define HYSTERESIS_MAX_DEFAULT    95
#define RUNTIME_DEFAULT           8
#define TARGET_SOC_DEFAULT        80

char *localhost = "localhost";
char *weconnect = "weconnect";
int go = 1;

typedef struct _mqttattr {
    char *mqtt_host;
    char *mqtt_user;
    char *mqtt_password;
    int mqtt_port;
    int qos;
    int retain;
    char cid[20];
    int verbose;
    int tlen;
    char **topics;
    int mode;
    char *prefix;
    char vin[18];
    int reduced;
    int hysteresis_min;
    int hysteresis_max;
    int battery_max;
    int battery;
    char chargingState[20];
    int currentSOC_pct;
    int targetSOC_pct;
    int cruisingRangeElectric_km;
    char maxChargeCurrentAC[20];
    int pv_solar_power;
    int pv_home_power;
    int pv_grid_power;
    int pv_battery_power;
    int pv_battery_soc;
    int km;
    int pump;
    int connected;
    int runtime;
    char *topic_control_charging;
    char *topic_control_current;
    char *topic_control_update_interval;
    char *topic_control_target_soc;
} mqttattr;

mqttattr create_mqttattr() {
    mqttattr c;

    c.mqtt_host = NULL;
    c.mqtt_port = 1883;
    c.mqtt_user = NULL;
    c.mqtt_password = NULL;
    c.qos = 0;
    c.retain = 0;
    c.verbose = 0;
    c.tlen = 0;
    c.topics = NULL;
    c.prefix = NULL;
    strcpy(c.vin, "");
    c.reduced = 0;
    c.hysteresis_min = HYSTERESIS_MIN_DEFAULT;
    c.hysteresis_max = HYSTERESIS_MAX_DEFAULT;
    c.battery_max = BATTERY_MAX_POWER_DEFAULT;
    c.battery = 0;
    c.currentSOC_pct = -1;
    c.targetSOC_pct = -1;
    strcpy(c.chargingState, "");
    c.cruisingRangeElectric_km = -1;
    strcpy(c.maxChargeCurrentAC, "");
    c.pv_solar_power = -1;
    c.pv_home_power = 0;
    c.pv_grid_power = 0;
    c.pv_battery_power = 0;
    c.pv_battery_soc = TARGET_SOC_DEFAULT;
    c.km = -1;
    c.pump = 0;
    c.connected = 0;
    c.runtime = RUNTIME_DEFAULT;
    c.topic_control_charging = NULL;
    c.topic_control_current = NULL;
    c.topic_control_update_interval = NULL;
    c.topic_control_target_soc = NULL;
    return(c);
}

int mstrcpy(char **str, const char *fmt, ...){
    if (str == NULL) return(0);

    va_list ap;

    va_start(ap, fmt);
    int space = 1 + vsnprintf(NULL, 0, fmt, ap);

    va_end(ap);

    char *n = realloc(*str, space);

    if (n == NULL) {
        if (*str) free(*str);
        *str = NULL;
        return(0);
    }
    *str = n;

    va_start(ap, fmt);
    int i = vsnprintf(*str, space, fmt, ap);

    va_end(ap);

    if (i < 0) {
        if (*str) free(*str);
        *str = NULL;
        return(0);
    }

    return(i);
}

int add_topic(mqttattr *mqtta, char *topic) {
    if (mosquitto_sub_topic_check(topic) == MOSQ_ERR_SUCCESS) {
        char **p;
        p = (char**)realloc(mqtta->topics, (mqtta->tlen + 1) * sizeof(char*));
        if (p != NULL) {
            mqtta->topics = p;
            mqtta->topics[mqtta->tlen] = topic;
            mqtta->tlen++;
            return(mqtta->tlen);
        }
    } else if (mqtta->verbose) printf("Warning: '%s' is not a valid topic\n", topic);
    return(0);
}

void destroy_mqttattr(mqttattr *mqtta) {
    if (mqtta) {
        if (mqtta->topics) free(mqtta->topics);
        mqtta->tlen = 0;
    }
    return;
}

char *now(char *ts) {
    struct tm *timeinfo;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    timeinfo = localtime(&tv.tv_sec);
    sprintf(ts, "%.4d%.2d%.2d%.2d%.2d%.2d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return(ts);
}

int publish(char *cid, char *host, int port, char *topic, char *payload, int qos, int retain, int verbose, char *user, char *password){
    int rc = 1;
    struct mosquitto *mosq = NULL;
    bool r = retain?true:false;
    char timestamp[24];

    now(timestamp);

    if (verbose) printf("[%s] publish: cid->%s< host->%s< port->%d< topic->%s< payload->%s< qos->%d< retain->%d<\n", timestamp, cid, host, port, topic, payload, qos, retain);

    mosq = mosquitto_new(cid, true, NULL);
    if (mosq) {
        if (user && password) mosquitto_username_pw_set(mosq, user, password);
        if (mosquitto_connect(mosq, host, port, 10)) {
            if (verbose) printf("[%s] publish: Error mosquitto_connect failed.\n", timestamp);
            mosquitto_destroy(mosq);
            mosq = NULL;
        } else  if (verbose) printf("[%s] publish: MQTT broker connected.\n", timestamp);
    } else  if (verbose) printf("[%s] publish: Error mosquitto_new or mosquitto_connect failed.\n", timestamp);
    if (mosq) {
        rc = mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, qos, r);
        if (rc) {
            if (verbose) printf("[%s] publish: Error >%s<\n", timestamp, mosquitto_strerror(rc));
        } else  if (verbose) printf("[%s] publish: successfully done.\n", timestamp);
    }
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);

    return(rc);
}

static void catch_signal(int sig) {
    printf("\nProgram stopped by user.");
    go = 0;
}

int regex_match(char *string, char *pattern) {
    regex_t preg;
    size_t nmatch = 1;
    regmatch_t pmatch[nmatch];

    if (regcomp(&preg, pattern, REG_EXTENDED | REG_NEWLINE))
        return(0);
    if (regexec(&preg, string, nmatch, pmatch, 0) == REG_NOMATCH) {
        regfree(&preg);
        return(0);
    } else {
        regfree(&preg);
        return(1);
    }
}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
    char timestamp[24];

    now(timestamp);
    mqttattr *mqtta = obj;

    if (!result) {
        mosquitto_subscribe_multiple(mosq, NULL, mqtta->tlen, (char *const *const)mqtta->topics, mqtta->qos, 0, NULL);

        if (mqtta->verbose) {
            printf("[%s] Source MQTT broker connected.\n", timestamp);
            int i;
            for (i = 0; i < mqtta->tlen; i++)
                printf("[%s] topic '%s' subscribed.\n", timestamp, mqtta->topics[i]);
        }
    }
    return;
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    char timestamp[24];
    mqttattr *mqtta = obj;
    int action = 0;
    int power_available = 0;
    static time_t ts_last = 0;
    static time_t ts_start = 0;

    if (ts_start == 0) ts_start = time(NULL);

    now(timestamp);

    if (mqtta->verbose) printf("[%s] >%s< >%.*s<\n", timestamp, message->topic, message->payloadlen, (char*)message->payload);

    if (!strcmp(message->topic, "e3dc/solar/power")) {
        mqtta->pv_solar_power = atoi((char*)message->payload);
        if (mqtta->pv_solar_power == 0) {
            printf("\nStop program because solar power is 0.\n");
            go = 0;
        } else action = 1;
    } else if (!strcmp(message->topic, "e3dc/home/power"))
        mqtta->pv_home_power = atoi((char*)message->payload);
    else if (!strcmp(message->topic, "e3dc/grid/power"))
        mqtta->pv_grid_power = atoi((char*)message->payload);
    else if (!strcmp(message->topic, "e3dc/battery/power"))
        mqtta->pv_battery_power = atoi((char*)message->payload);
    else if (!strcmp(message->topic, "e3dc/battery/soc"))
        mqtta->pv_battery_soc = atoi((char*)message->payload);
    else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/chargingStatus/chargingState")) {
        if ((message->payloadlen > 0) && (message->payloadlen < 20)) {
            strncpy(mqtta->chargingState, (char*)message->payload, message->payloadlen);
            mqtta->chargingState[message->payloadlen] = 0;
            action = 1;
        }
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/batteryStatus/currentSOC_pct")) {
        if (message->payloadlen > 0) mqtta->currentSOC_pct = atoi((char*)message->payload);
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/chargingSettings/targetSOC_pct")) {
        if (message->payloadlen > 0) mqtta->targetSOC_pct = atoi((char*)message->payload);
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/batteryStatus/cruisingRangeElectric_km")) {
        if (message->payloadlen > 0) mqtta->cruisingRangeElectric_km = atoi((char*)message->payload);
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/chargingSettings/maxChargeCurrentAC")) {
        if ((message->payloadlen > 0) && (message->payloadlen < 20)) {
            strncpy(mqtta->maxChargeCurrentAC, (char*)message->payload, message->payloadlen);
            mqtta->maxChargeCurrentAC[message->payloadlen] = 0;
        }
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/charging/plugStatus/plugConnectionState")) {
        if (!strncmp((char*)message->payload, "connected", message->payloadlen)) mqtta->connected = 1;
        if ((!strncmp((char*)message->payload, "disconnected", message->payloadlen)) && mqtta->connected) {
            printf("\nStop program because car has been disconnected.\n");
            go = 0;
        }
    } else if (strstr(message->topic, mqtta->vin) && strstr(message->topic, "domains/measurements/odometerStatus/odometer")) {
        if (mqtta->km < 0) {
            mqtta->km = atoi((char*)message->payload);
            printf("\nkm = %d\n", mqtta->km);
        }
        if (mqtta->km && (atoi((char*)message->payload) > mqtta->km)) {
            printf("\nStop program because car is moving (km = %d).\n", atoi((char*)message->payload));
            go = 0;
        }
    }

    if (time(NULL) > ts_start + (3600 * mqtta->runtime)) {
        printf("\nStop program because runtime has expired.\n");
        go = 0;
    }

    if (mqtta->pv_solar_power <= 0)
        power_available = 0;
    else if ((mqtta->pv_battery_soc >= mqtta->hysteresis_max) && (mqtta->pump == 0)) {
        mqtta->pump = 1;
        power_available = mqtta->pv_solar_power - mqtta->pv_home_power + mqtta->battery_max;
    } else if ((mqtta->pv_battery_soc < mqtta->hysteresis_min) && (mqtta->pump == 1)) {
        mqtta->pump = 0;
        power_available = mqtta->pv_solar_power - mqtta->pv_home_power - mqtta->battery;
    } else if (mqtta->pump == 1)
        power_available = mqtta->pv_solar_power - mqtta->pv_home_power + mqtta->battery_max;
    else
        power_available = mqtta->pv_solar_power - mqtta->pv_home_power - mqtta->battery;

    printf("\r                                                                                                ");
    printf("\r[%s] SOC=%d(%d) Range=%dkm surplus=%dW State: %s %s", timestamp, mqtta->currentSOC_pct, mqtta->targetSOC_pct, mqtta->cruisingRangeElectric_km, power_available, mqtta->chargingState, mqtta->maxChargeCurrentAC);
    fflush(NULL);

    if (action && ((time(NULL) - ts_last) > INTERVAL_FAST)) {
        ts_last = time(NULL);

        // Power
        if (mqtta->reduced && strcmp(mqtta->maxChargeCurrentAC, "reduced")) {
            publish(mqtta->cid, mqtta->mqtt_host, mqtta->mqtt_port, mqtta->topic_control_current, (char*)"reduced", mqtta->qos, mqtta->retain, mqtta->verbose, mqtta->mqtt_user, mqtta->mqtt_password);
            printf("\n[%s] published: switch to reduced charging power (reduced mode)\n", timestamp);
        } else if (!mqtta->reduced && !strcmp(mqtta->chargingState, "charging") && (power_available > REDUCED_CHARGE_POWER)) {
            if (strcmp(mqtta->maxChargeCurrentAC, "maximum")) {
                publish(mqtta->cid, mqtta->mqtt_host, mqtta->mqtt_port, mqtta->topic_control_current, (char*)"maximum", mqtta->qos, mqtta->retain, mqtta->verbose, mqtta->mqtt_user, mqtta->mqtt_password);
                printf("\n[%s] published: switch to maximum charging power\n", timestamp);
            }
        } else if (!mqtta->reduced && !strcmp(mqtta->chargingState, "charging") && (power_available < 0)) {
            if (strcmp(mqtta->maxChargeCurrentAC, "reduced")) {
                publish(mqtta->cid, mqtta->mqtt_host, mqtta->mqtt_port, mqtta->topic_control_current, (char*)"reduced", mqtta->qos, mqtta->retain, mqtta->verbose, mqtta->mqtt_user, mqtta->mqtt_password);
                printf("\n[%s] published: switch to reduced charging power\n", timestamp);
            }
        }

        // Charging
        if ((!strcmp(mqtta->chargingState, "readyForCharging")) && (power_available > REDUCED_CHARGE_POWER)) {
            publish(mqtta->cid, mqtta->mqtt_host, mqtta->mqtt_port, mqtta->topic_control_charging, (char*)"start", mqtta->qos, mqtta->retain, mqtta->verbose, mqtta->mqtt_user, mqtta->mqtt_password);
            printf("\n[%s] published: start charging\n", timestamp);
        } else if (!strcmp(mqtta->chargingState, "charging") && (power_available < -250) && (!strcmp(mqtta->maxChargeCurrentAC, "reduced"))) {
            publish(mqtta->cid, mqtta->mqtt_host, mqtta->mqtt_port, mqtta->topic_control_charging, (char*)"stop", mqtta->qos, mqtta->retain, mqtta->verbose, mqtta->mqtt_user, mqtta->mqtt_password);
            printf("\n[%s] published: stop charging\n", timestamp);
        }
    }

    return;
}

int main(int argc, char **argv) {
    struct mosquitto *mosq;
    int rc = 0;
    int i = 0;
    char buffer[16];

    if (signal(SIGINT, catch_signal) == SIG_ERR) {
        printf("error: signal couldn't be set.\n");
        exit(0);
    }

    mqttattr mqtta = create_mqttattr();

    while (i < argc) {
        if ((!strcmp(argv[i], "--host")) && (i + 1 < argc)) mqtta.mqtt_host = argv[++i];
        if ((!strcmp(argv[i], "--port")) && (i + 1 < argc)) mqtta.mqtt_port = atoi(argv[++i]);
        if ((!strcmp(argv[i], "--user")) && (i + 1 < argc)) mqtta.mqtt_user = argv[++i];
        if ((!strcmp(argv[i], "--password")) && (i + 1 < argc)) mqtta.mqtt_password = argv[++i];
        if ((!strcmp(argv[i], "--qos")) && (i + 1 < argc)) mqtta.qos = atoi(argv[++i]);
        if ((!strcmp(argv[i], "--retain")) && (i + 1 < argc)) mqtta.retain = atoi(argv[++i]);
        if ((!strcmp(argv[i], "--vin")) && (i + 1 < argc)) strncpy(mqtta.vin, argv[++i], 18);
        if ((!strcmp(argv[i], "--battery")) && (i + 1 < argc)) mqtta.battery = abs(atoi(argv[++i]));
        if ((!strcmp(argv[i], "--battery_max")) && (i + 1 < argc)) mqtta.battery_max = abs(atoi(argv[++i]));
        if (!strcmp(argv[i], "--no_hysteresis")) mqtta.pump = -1;
        if ((!strcmp(argv[i], "--hysteresis_min")) && (i + 1 < argc)) mqtta.hysteresis_min = abs(atoi(argv[++i]));
        if ((!strcmp(argv[i], "--hysteresis_max")) && (i + 1 < argc)) mqtta.hysteresis_max = abs(atoi(argv[++i]));
        if ((!strcmp(argv[i], "--prefix")) && (i + 1 < argc)) mqtta.prefix = argv[++i];
        if ((!strcmp(argv[i], "--target_soc")) && (i + 1 < argc)) mqtta.targetSOC_pct = abs(atoi(argv[++i]));
        if (!strcmp(argv[i], "--reduced")) mqtta.reduced = 1;
        if (!strcmp(argv[i], "-v")) mqtta.verbose = 1;
        if (!strcmp(argv[i], "--runtime")) mqtta.runtime = abs(atoi(argv[++i]));
        i++;
    }

    if (!mqtta.mqtt_host) mqtta.mqtt_host = localhost;
    if (!mqtta.prefix) mqtta.prefix = weconnect;
    if ((mqtta.qos < 0) || (mqtta.qos > 2)) mqtta.qos = 0;
    if ((mqtta.targetSOC_pct < 30) || (mqtta.targetSOC_pct > 100) || (mqtta.targetSOC_pct % 10)) mqtta.targetSOC_pct = TARGET_SOC_DEFAULT;
    if ((mqtta.hysteresis_min < 0) || (mqtta.hysteresis_min > 100 )) mqtta.hysteresis_min = HYSTERESIS_MIN_DEFAULT;
    if ((mqtta.hysteresis_max < 0) || (mqtta.hysteresis_max > 100 )) mqtta.hysteresis_max = HYSTERESIS_MAX_DEFAULT;
    if (mqtta.hysteresis_min >= mqtta.hysteresis_max) {
        mqtta.hysteresis_min = HYSTERESIS_MIN_DEFAULT;
        mqtta.hysteresis_max = HYSTERESIS_MAX_DEFAULT;
    }
    if ((mqtta.runtime < 1) || (mqtta.runtime > 10)) mqtta.runtime = RUNTIME_DEFAULT;

    add_topic(&mqtta, "+/vehicles/+/domains/charging/#");
    add_topic(&mqtta, "+/vehicles/+/domains/measurements/#");
    add_topic(&mqtta, "e3dc/+/power");
    add_topic(&mqtta, "e3dc/battery/soc");

    if (strlen(mqtta.vin) != 17) {
        printf("chargemanager - charging an electric car depending on the availability of surplus energy from the photovoltaic\n\nusage: %s\n", basename(argv[0]));
        printf("\t\t\t--vin <vin> vehicle identification number\n");
        printf("\t\t\t--runtime <1..10> program is terminated when the runtime has expired (specified in hours) (default: %d)\n", RUNTIME_DEFAULT);
        printf("\t\t\t--host <host> of the MQTT broker (default: %s)\n", localhost);
        printf("\t\t\t--port <port> of the MQTT broker (default: 1883)\n");
        printf("\t\t\t--user <user> for login to the MQTT broker\n");
        printf("\t\t\t--password <password> for login to the MQTT broker\n");
        printf("\t\t\t--qos <0..2> QOS of the MQTT broker (default: 0)\n");
        printf("\t\t\t--retain <0,1> retain the published data in the MQTT broker (default: 0)\n");
        printf("\t\t\t--battery <power in W> power to remain for house battery charging (default: 0)\n");
        printf("\t\t\t--battery_max <power in W> max power to discharge the house battery (default: %d)\n", BATTERY_MAX_POWER_DEFAULT);
        printf("\t\t\t--hysteresis_min <0..100> house battery supports car charging min SOC value (default: %d)\n", HYSTERESIS_MIN_DEFAULT);
        printf("\t\t\t--hysteresis_min <0..100> house battery supports car charging max SOC value (default: %d)\n", HYSTERESIS_MAX_DEFAULT);
        printf("\t\t\t--no_hysteresis no support by the house battery\n");
        printf("\t\t\t--prefix <prefix of the WeConnect topics> (default: %s)\n", weconnect);
        printf("\t\t\t--target_soc <30,40,50,..,100> target SOC of the car battery (default: %d)\n", TARGET_SOC_DEFAULT);
        printf("\t\t\t--reduced charge with reduced power\n");
        printf("\t\t\t-v verbose mode\n");
        printf("\nExample: %s --host localhost --vin WVXZZZ12345678900 --no_hysteresis --prefix weconnect\n", basename(argv[0]));
        printf("\nError: VIN must have 17 characters\n");
        return(1);
    }

    printf("chargemanager: battery_max = %d ", mqtta.battery_max);
    if (mqtta.pump == -1) printf("hysteresis = off "); else printf("hysteresis_min = %d hysteresis_max = %d ", mqtta.hysteresis_min, mqtta.hysteresis_max);
    printf("runtime = %d target_soc = %d\n", mqtta.runtime, mqtta.targetSOC_pct);

    sprintf(mqtta.cid, "charger/%d", getpid());

    mstrcpy(&mqtta.topic_control_current, "%s/vehicles/%s/domains/charging/chargingSettings/maxChargeCurrentAC_writetopic", mqtta.prefix, mqtta.vin);
    mstrcpy(&mqtta.topic_control_charging, "%s/vehicles/%s/controls/charging_writetopic", mqtta.prefix, mqtta.vin);
    mstrcpy(&mqtta.topic_control_update_interval, "%s/mqtt/weconnectUpdateInterval_s_writetopic", mqtta.prefix);
    mstrcpy(&mqtta.topic_control_target_soc, "%s/vehicles/%s/domains/charging/chargingSettings/targetSOC_pct_writetopic", mqtta.prefix, mqtta.vin);

    mosquitto_lib_init();

    sprintf(buffer, "%d", INTERVAL_FAST);
    publish(mqtta.cid, mqtta.mqtt_host, mqtta.mqtt_port, mqtta.topic_control_update_interval, buffer, mqtta.qos, mqtta.retain, mqtta.verbose, mqtta.mqtt_user, mqtta.mqtt_password);

    sprintf(buffer, "%d", mqtta.targetSOC_pct);
    publish(mqtta.cid, mqtta.mqtt_host, mqtta.mqtt_port, mqtta.topic_control_target_soc, buffer, mqtta.qos, mqtta.retain, mqtta.verbose, mqtta.mqtt_user, mqtta.mqtt_password);

    mosq = mosquitto_new(mqtta.cid, true, &mqtta);

    if (mosq) {
        mosquitto_connect_callback_set(mosq, connect_callback);
        mosquitto_message_callback_set(mosq, message_callback);

        if (mqtta.verbose) {
            int major, minor, revision;
            mosquitto_lib_version(&major, &minor, &revision);
            printf("%s (libmosquitto %d.%d.%d)\n", basename(argv[0]), major, minor, revision);
            printf("Connecting (%s) to %s:%d with qos=%d\n", mqtta.cid, mqtta.mqtt_host, mqtta.mqtt_port, mqtta.qos);
        }

        if (mqtta.mqtt_user && mqtta.mqtt_password) mosquitto_username_pw_set(mosq, mqtta.mqtt_user, mqtta.mqtt_password);
        rc = mosquitto_connect(mosq, mqtta.mqtt_host, mqtta.mqtt_port, 60);
        if (!rc) {
            while (go) {
                rc =  mosquitto_loop(mosq, -1, 1);
                if (go && rc) {
                    usleep(10000);
                    mosquitto_reconnect(mosq);
                }
            }
        } else  if (mqtta.verbose) printf("Error: Could not connect to '%s:%d'\n", mqtta.mqtt_host, mqtta.mqtt_port);

        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
    }

    printf("\n");

    sprintf(buffer, "%d", INTERVAL_DEFAULT);
    publish(mqtta.cid, mqtta.mqtt_host, mqtta.mqtt_port, mqtta.topic_control_update_interval, buffer, mqtta.qos, mqtta.retain, mqtta.verbose, mqtta.mqtt_user, mqtta.mqtt_password);

    if (!strcmp(mqtta.chargingState, "charging"))
        publish(mqtta.cid, mqtta.mqtt_host, mqtta.mqtt_port, mqtta.topic_control_charging, (char*)"stop", mqtta.qos, mqtta.retain, mqtta.verbose, mqtta.mqtt_user, mqtta.mqtt_password);

    mosquitto_lib_cleanup();

    destroy_mqttattr(&mqtta);

    return(0);
}
