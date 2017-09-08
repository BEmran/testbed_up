/*
 * File:   main.cpp
 * Author: Bara Emran
 * Created on September 9, 2017, 12:44 PM
 */
#include "../include/testbed_up/up_basic.h"

/*****************************************************************************************
main: Run main function
 ****************************************************************************************/

int main(int argc, char** argv)
{
    printf("Start Program\n");
    signal(SIGINT, ctrlCHandler);

    //------------------------------------- Define main variables --------------------------------
    struct dataStruct data;
    data.argc = argc;
    data.argv = argv;
    data.yaw_fold = 0;

    //----------------------------------------- Start threads ---------------------------------------
    pthread_create(&_Thread_Sensors, NULL, sensorsThread, (void *) &data);
    pthread_create(&_Thread_RosNode, NULL, rosNodeThread, (void *) &data);

    //------------------------------------------  Main loop ------------------------------------------
    while (!_CloseRequested) {
        printf("main\n");
        sleep(1);
    }

    //---------------------------------------- Exit procedure -------------------------------------
    pthread_cancel(_Thread_Sensors);
    pthread_cancel(_Thread_RosNode);
    printf("Close program\n");

    return 0;
}

/*****************************************************************************************
 ctrlCHandler: Detect ctrl+c to quit program
 ****************************************************************************************/
void ctrlCHandler(int signal) {
    _CloseRequested = true;
    printf("Ctrl+c have been detected\n");
}

/*****************************************************************************************
 sensorsThread: read encoderes
 *****************************************************************************************/
void *sensorsThread(void *data) {
    printf("Start Sensors thread\n");
    struct dataStruct *my_data;
    my_data = (struct dataStruct *) data;

    //------------------------------------ Initialize USB4 sensor-----------------------------------
    printf("Initialize USB4 Sensor\n");
    short deviceCount = 0;
    int iResult = USB4_Initialize(&deviceCount); // Initialize the USB4 driver
    if (iResult != USB4_SUCCESS) {
        ROS_INFO("USB4 can't be initialized, USB4 error #%d\n", iResult);
        exit(EXIT_FAILURE);
    } else
        ROS_INFO("USB4 is initialized\n");

    //-------------------------------------- Configure encoders -----------------------------------
    Encoder En0 (0, 0, 10000, QUAD_X4);
    Encoder En1 (0, 1, 10000, QUAD_X4);
    Encoder En2 (0, 2, 10000, QUAD_X4);

    //------------------------------------------  Main loop --------------------------------------------
    SamplingTime st(_SENSOR_FREQ);
    float dt, dtsumm = 0;
    while (!_CloseRequested) {

        //-------------------------------------- Read Sensor -----------------------------------------
        my_data->encoderes[0] = En0.getAngleRad() * RSign;
        my_data->encoderes[1] = En1.getAngleRad() * PSign;
        my_data->encoderes[2]  = unfoldYaw(En2.getAngleRad() * YSign, my_data->encoderes[2], &my_data->yaw_fold);

        //----------------------------------- Display values ------------------------------------------

        dt = st.tsCalculat();
        if (dtsumm > 1) {
            dtsumm = 0;
            ROS_INFO("er = %+2.2f\t ep = %+2.2f\t ew = %+2.2f\t", my_data->encoderes[0], my_data->encoderes[1],  my_data->encoderes[2]);
        }
    }

    //---------------------------------------- Exit procedure ----------------------------------------
    printf("Exit sensor thread\n");
    pthread_exit(NULL);
}

/*****************************************************************************************
 rosNodeThread: ROS Node thread
 *****************************************************************************************/
void *rosNodeThread(void *data) {
    printf("Start ROS Node thread\n");
    struct dataStruct *my_data;
    my_data = (struct dataStruct *) data;

    //---------------------------------------- Initialize ROS -----------------------------------------
    ros::init(my_data->argc,my_data->argv,"up_basic");
    ros::NodeHandle n;
    ros::Publisher encoder_pub = n.advertise <geometry_msgs::Vector3Stamped>("testbed/sensors/encoderes", 1000);
    ros::Rate loop_rate(_ROS_FREQ);
    geometry_msgs::Vector3Stamped encoderes;

    //------------------------------------------  Main loop -------------------------------------------
    while (ros::ok() && !_CloseRequested)
    {
        //------------------------------  Prepare encoderes msg ----------------------------------
        encoderes.header.stamp = ros::Time::now();
        encoderes.header.seq++;
        encoderes.vector.x = my_data->encoderes[0];
        encoderes.vector.y = my_data->encoderes[1];
        encoderes.vector.z = my_data->encoderes[2];
        encoder_pub.publish(encoderes);

        ros::spinOnce();
        loop_rate.sleep();
    }

    //---------------------------------------- Exit procedure ---------------------------------------
    ctrlCHandler(0);
    printf("Exit ROS Node thread\n");
    pthread_exit(NULL);
}

/*****************************************************************************************
 unfoldYaw: Unfold yaw angle
 *****************************************************************************************/
float unfoldYaw (float yaw_new, float yaw_old, int* n){
    if ((yaw_new - yaw_old) > 6.0)
        *n = *n  - 1;
    else if ((yaw_new - yaw_old) < -6.0)
        *n = *n  + 1;
    return (*n * 2 * PI + yaw_new);
}