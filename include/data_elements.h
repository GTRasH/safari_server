/*	# # # # # # # # # # # # # # # # # # # # # # # # #
 *	#	Data Elements								#
 * 	#												#
 *	#	Author:		Sebastian Neutsch				#
 * 	#	Mail:		Sebastian.Neutsch@t-online.de	#
 *	#	Project:	SAFARI HfTL Prototype			#
 *	#	Licence:	GPLv3							#
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
*/

/* 
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
 *	#				DATA ELEMENTS					#
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
 */
 
/** \brief string data elements
*
*/
typedef char DescriptiveName[63];

/** \brief integer data elements
*
*/
typedef int32_t	Latitude,
				Longitude,
				Elevation;

/** \brief unsigned integer data elements
*
*/
typedef uint32_t MinuteOfTheYear;

/** \brief integer data elements (0..65535)
*
*/
typedef uint16_t	Velocity, 
					RoadRegulatorID,
					IntersectionID,
					LaneWidth;

/** \brief integer data elements (0..256)
*
*/
typedef uint8_t	MsgCount,
				LaneID,
				ApproachID;

 /** \brief data element SpeedLimitType
*
*/
typedef enum {
	unknown,									/*!< speed limit type not available */
	maxSpeedInSchoolZone,						/*!< Only sent when the limit is active */
	maxSpeedInSchoolZoneWhenChildrenArePresent,	/*!< Sent at any time */
	maxSpeedInConstructionZone,					/*!< used for work zones, incident zones etc.
													 where a reduced speed is present */
	vehicleMinSpeed,							
	vehicleMaxSpeed,
	vehicleNightMaxSpeed,
	truckMinSpeed,
	truckMaxSpeed,
	truckNightMaxSpeed,
	vehiclesWithTrailersMinSpeed,
	vehiclesWithTrailersMaxSpeed,
	vehiclesWithTrailersNightMaxSpeed
} SpeedLimitType;
