/*	# # # # # # # # # # # # # # # # # # # # # # # # #
 *	#	MAP Message Structure						#
 * 	#												#
 *	#	Author:		Sebastian Neutsch				#
 * 	#	Mail:		Sebastian.Neutsch@t-online.de	#
 *	#	Project:	SAFARI HfTL Prototype			#
 *	#	Licence:	GPLv3							#
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
*/

/*	# # # # # # # # # # # # # # # # # # # # # # # # #
 * 	#				DATA FRAMES						#
 * 	# # # # # # # # # # # # # # # # # # # # # # # # #
 */
 
 /** \brief data frame Position3D
 * 
 */
typedef struct {
	Latitude	lat;		/*!< in 1/10th micro degress */
	Longitude	longi;		/*!< in 1/10th micro degress */
	Elevation	elevation;	/*!< in 10 cm units */
} Position3D;

/** \brief data frame IntersectionReferenceID
 * 
 */
typedef struct {
	RoadRegulatorID	region;
	IntersectionID	id;		/*!< values 0 through 255 are allocated for testing purposes */
} IntersectionReferenceID;


/** \brief data frame RegulatorySpeedLimit
 * 
 */
typedef struct {
	SpeedLimitType	type;
	Velocity		speed;	/*!< units of 0.02 m/s
								 the value 8192 indicates that velocity is unavailable */
} RegulatorySpeedLimit;

/** \brief data frame SpeedLimitList
 * 
 */
typedef RegulatorySpeedLimit *SpeedLimitList[9];

/** \brief data frame OverlayLaneList
 * 
 */
typedef LaneID *OverlayLaneList[5];

/** \brief data frame GenericLane
 * 
 */ 
typedef struct {
	LaneID				laneID;	/*!< The value 0 shall be used when the lane ID is not available
									 or not known. The value 255 is reserved for future use. */
	DescriptiveName		name;	/*!< for debug only */
	ApproachID			ingressApproach;	/*!< zero to be used when valid value is unkown */
	ApproachID			egressApproach;		/*!< zero to be used when valid value is unkown */
	/*!< LaneAttributes		laneAttributes;		LaneAttributes are not implemented yet
	AllowedManeuvers	maneuvers;				AllowedManeuvers are not implemented yet
	NodeListXY			nodeList;				NodeListXY is not implemented yet
	ConnectsToList		connectsTo;				ConnectsToList is not implemented yet */
	OverlayLaneList		overlays;
} GenericLane;

/** \brief data frame LaneList
 * 
 */
typedef GenericLane *LaneList[255];

 /** \brief data frame IntersectionGeometry
 * 
 */
typedef struct {
	DescriptiveName			name;			/*!< for debug only */
	IntersectionReferenceID	id;
	MsgCount				revision;
	Position3D				refpoint;
	LaneWidth				laneWidth;		/*!< units of 1 cm */
	SpeedLimitList			speedLimits;
	LaneList				laneSet;
} IntersectionGeometry;

/** \brief data frame IntersectionGeometryList
 * 
 */
typedef IntersectionGeometry *IntersectionGeometryList[32];

/** \brief data frame MapData
 * 
 */
typedef struct {
	MinuteOfTheYear				timestamp;
	MsgCount 					msgIssueRevision;
	IntersectionGeometryList	intersections;
} MapData;
