/*	# # # # # # # # # # # # # # # # # # # # # # # # #
 *	#	SPAT Message Structure						#
 * 	#												#
 *	#	Author:		Sebastian Neutsch				#
 * 	#	Mail:		Sebastian.Neutsch@t-online.de	#
 *	#	Project:	SAFARI HfTL Prototype			#
 *	#	Licence:	GPLv3							#
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
 * 
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
 *	#				DATA FRAME						#
 *	# # # # # # # # # # # # # # # # # # # # # # # # #
 */

 /** \brief data frame IntersectionState
 * 
 */
typedef IntersectionStatusObject {
	manualControlEnabled = 0000 0000 0000;
}

 /** \brief data frame IntersectionState
 * 
 */
typedef struct IntersectionState {
	DescriptiveName				name;
	IntersectionReferenceID		id;
	MsgCount					revision;
	IntersectionStatusObject	status;
	MovementList				states;
} IntersectionState;

 /** \brief data frame IntersectionStateList
 * 
 */
typedef IntersectionState *IntersectionStateList[32];

 /** \brief message SPAT
 * 
 */
typedef struct {
	MinuteOfTheYear			timestamp;
	DescriptiveName 		name;
	IntersectionStateList	intersections;
} SPAT;
