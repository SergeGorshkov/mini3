
amqp_socket_t *rsocket = NULL, *osocket = NULL;
amqp_rpc_reply_t resp;
amqp_connection_state_t rconn, oconn;

rd_kafka_t *rk, *ork;
rd_kafka_conf_t *conf, *oconf;                 /* Temporary configuration object */
rd_kafka_resp_err_t err;                       /* librdkafka API error code */
char errstr[512];                              /* librdkafka API error reporting buffer */
char brokers[256];                             /* Argument: broker list */
char groupid[32];                              /* Argument: Consumer group id */
char *topics;                                  /* Argument: list of topics to subscribe to */
int topic_cnt;                                 /* Number of topics to subscribe to */
rd_kafka_topic_partition_list_t *subscription; /* Subscribed topics */

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    if (rkmessage->err)
        ;
}

void listen_queues(void)
{
    if (strcmp(broker, "RabbitMQ") == 0)
    {
        rconn = amqp_new_connection();
        rsocket = amqp_tcp_socket_new(rconn);
        if (!rsocket)
            logger(ERROR, "Cannot create RabbitMQ socket", "", 0);
        int status = amqp_socket_open(rsocket, broker_host, atoi(broker_port));
        if (status)
            logger(ERROR, "Cannot bind to RabbitMQ socket", "", 0);
        resp = amqp_login(rconn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, broker_user, broker_password);
        if (resp.reply_type != AMQP_RESPONSE_NORMAL)
            logger(ERROR, "Cannot login to RabbitMQ", "", 0);
        amqp_channel_open(rconn, 1);
        resp = amqp_get_rpc_reply(rconn);
        if (resp.reply_type != AMQP_RESPONSE_NORMAL)
            logger(ERROR, "Cannot open RabbitMQ channel", "", 0);
        amqp_basic_consume(rconn, 1, amqp_cstring_bytes(broker_queue), amqp_empty_bytes,
                           0, 0, 0, amqp_empty_table);
        resp = amqp_get_rpc_reply(rconn);
        if (resp.reply_type != AMQP_RESPONSE_NORMAL)
            logger(ERROR, "Cannot get reply from RabbitMQ", "", 0);
        //  	amqp_channel_close(rconn, 1, AMQP_REPLY_SUCCESS);
        //  	amqp_connection_close(rconn, AMQP_REPLY_SUCCESS);
        //  	amqp_destroy_connection(rconn);
        // Открываем выходную очередь
        oconn = amqp_new_connection();
        osocket = amqp_tcp_socket_new(oconn);
        if (!osocket)
            logger(ERROR, "Cannot create RabbitMQ socket", "", 0);
        status = amqp_socket_open(osocket, broker_host, atoi(broker_port));
        if (status)
            logger(ERROR, "Cannot bind to RabbitMQ socket", "", 0);
        resp = amqp_login(oconn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN,
                          broker_user, broker_password);
        if (resp.reply_type != AMQP_RESPONSE_NORMAL)
            logger(ERROR, "Cannot login to RabbitMQ", "", 0);
        amqp_channel_open(oconn, 1);
        resp = amqp_get_rpc_reply(oconn);
        if (resp.reply_type != AMQP_RESPONSE_NORMAL)
            logger(ERROR, "Cannot open RabbitMQ channel", "", 0);
    }
    else if (strcmp(broker, "ApacheKafka") == 0)
    {
        strcpy(brokers, broker_host);
        if (strcmp(broker_port, "9092") != 0)
        {
            strcat(brokers, ":");
            strcat(brokers, broker_port);
        }
        strcpy(groupid, "mini3");
        topics = broker_queue;
        topic_cnt = 1;

        conf = rd_kafka_conf_new();
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
        {
            rd_kafka_conf_destroy(conf);
            logger(ERROR, "Cannot login to Kafka", errstr, 0);
        }
        if (rd_kafka_conf_set(conf, "group.id", groupid,
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
        {
            rd_kafka_conf_destroy(conf);
            logger(ERROR, "Cannot set Kafka group id", errstr, 0);
        }
        if (rd_kafka_conf_set(conf, "auto.offset.reset", "earliest",
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
        {
            rd_kafka_conf_destroy(conf);
            logger(ERROR, "Cannot set Kafka auto.offset.reset", errstr, 0);
        }
        rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
        if (!rk)
            logger(ERROR, "Cannot create Kafka consumer instance", errstr, 0);

        conf = NULL;
        rd_kafka_poll_set_consumer(rk);
        subscription = rd_kafka_topic_partition_list_new(topic_cnt);
        for (int i = 0; i < topic_cnt; i++)
            rd_kafka_topic_partition_list_add(subscription,
                                              topics,
                                              RD_KAFKA_PARTITION_UA);

        err = rd_kafka_subscribe(rk, subscription);
        if (err)
        {
            rd_kafka_topic_partition_list_destroy(subscription);
            rd_kafka_destroy(rk);
            logger(ERROR, "Cannot subscribe to Kafka queues", (char *)rd_kafka_err2str(err), 0);
        }
        printf("Subscribed to %d Kafka topic(s)\n", subscription->cnt);

        rd_kafka_topic_partition_list_destroy(subscription);
        strcpy(brokers, broker_host);
        if (strcmp(broker_port, "9092") != 0)
        {
            strcat(brokers, ":");
            strcat(brokers, broker_port);
        }
        oconf = rd_kafka_conf_new();
        if (rd_kafka_conf_set(oconf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
            logger(ERROR, "Cannot connect to outgoing Kafka ", errstr, 0);
        rd_kafka_conf_set_dr_msg_cb(oconf, dr_msg_cb);
        ork = rd_kafka_new(RD_KAFKA_PRODUCER, oconf, errstr, sizeof(errstr));
        if (!ork)
            logger(ERROR, "Failed to create Kafka producer", errstr, 0);
    }
    else return;

    // В отдельном потоке слушает Rabbit или Kafka
    printf("Start listening queues\n");
    if (fork() != 0)
        return;
    (void)signal(SIGCHLD, SIG_IGN);
    (void)signal(SIGHUP, SIG_IGN);
    logger(LOG, "mini-3 queues listener starting", "", getpid());
    while (true)
    {
        if (strcmp(broker, "RabbitMQ") == 0)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            amqp_maybe_release_buffers(rconn);
            res = amqp_consume_message(rconn, &envelope, NULL, 0);
            if (AMQP_RESPONSE_NORMAL != res.reply_type)
            {
                logger(ERROR, "Abnormal RabbitMQ reply", "", 0);
                break;
            }
            char *req = (char *)malloc(envelope.message.body.len + 1);
            if(!req) out_of_memory();
            memcpy(req, envelope.message.body.bytes, envelope.message.body.len);
            req[envelope.message.body.len] = 0;
            //logger(LOG, "Queue process\t", "Going to process request\n", 0);
            char *answer = process_request(req, 0, 0, 0, global_web_pip);
            //logger(LOG, "Queue process\t", "Returned from process request\n", 0);
            amqp_bytes_t message_bytes;
            message_bytes.len = strlen(answer) + 1;
            message_bytes.bytes = answer;
            if (answer)
            {
                amqp_basic_properties_t props;
                props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
                props.content_type = amqp_cstring_bytes("text/plain");
                props.delivery_mode = 2; // persistent delivery mode
                                         //logger(LOG, broker_output_queue, answer, 0);
                int code = amqp_basic_publish(oconn, 1, amqp_cstring_bytes(""),
                                              amqp_cstring_bytes(broker_output_queue), 0, 0, &props,
                                              message_bytes);
                if (code < 0)
                {
                    strcpy(answer, amqp_error_string2(code));
                    logger(LOG, "Error", answer, 0);
                }
                free(answer);
                amqp_basic_ack(rconn, 1, envelope.delivery_tag, 0);
                amqp_destroy_envelope(&envelope);
            }
        }
        else if (strcmp(broker, "ApacheKafka") == 0)
        {
            rd_kafka_message_t *rkm;
            rkm = rd_kafka_consumer_poll(rk, 100);
            if (!rkm)
                continue;
            if (rkm->err)
            {
                rd_kafka_message_destroy(rkm);
                continue;
            }

            if (rkm->payload)
            {
                char *rq = (char *)malloc(rkm->len + 1);
                if(!rq) out_of_memory();
                memcpy(rq, rkm->payload, rkm->len);
                rq[rkm->len] = 0;
                char *answer = process_request(rq, 0, 0, 0, global_web_pip);
                size_t len = strlen(answer);
                rd_kafka_resp_err_t err;
            retry:
                err = rd_kafka_producev(
                    ork,
                    RD_KAFKA_V_TOPIC(broker_output_queue),
                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                    RD_KAFKA_V_VALUE(answer, len),
                    RD_KAFKA_V_OPAQUE(NULL),
                    RD_KAFKA_V_END);

                if (err)
                {
                    logger(LOG, "Failed to produce to Kafka", (char *)rd_kafka_err2str(err), 0);
                    if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL)
                    {
                        rd_kafka_poll(ork, 1000);
                        goto retry;
                    }
                    rd_kafka_poll(ork, 100);
                }
            }
            rd_kafka_message_destroy(rkm);
            rd_kafka_flush(ork, 10 * 1000);
            /*        if (rd_kafka_outq_len(ork) > 0)
                fprintf(stderr, "%% %d message(s) were not delivered\n",
                        rd_kafka_outq_len(rk)); */
        }
        /*            rd_kafka_destroy(ork);
            rd_kafka_consumer_close(rk);
            rd_kafka_destroy(rk); */
    }
}
