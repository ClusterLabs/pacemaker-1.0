#ifndef _CMPI_UTILS_H
#define _CMPI_UTILS_H

#include <cmpidt.h>
#include <cmpift.h>
#include <cmpimacs.h>


/* for compatibility */
#ifndef CMPIVersion090
#define CMPIConst 

#undef CMGetCharPtr
#define CMGetCharPtr(x) ((char *)(x)->hdl)

#define CMIndicationMIStubExtensions(x)

#else /* CMPIVersion090 */
#define CMPIConst const

#endif /* CMPIVersion090 */

/* return ret if obj is a NULL CMPI Object */
#define RETURN_IFNULL_OBJ(obj, ret, n) do {                          \
                if ( CMIsNullObject(obj) ) {                         \
                       cl_log(LOG_ERR, "%s: CMPI Object %s is NULL", \
                                        __FUNCTION__, n);            \
                       return ret;                                   \
                }                                                    \
        } while (0)

/* return HA_FAIL if obj is a NULL CMPI Object */
#define RETURN_FAIL_IFNULL_OBJ(obj, n) RETURN_IFNULL_OBJ(obj, HA_FAIL, n)
#define RETURN_NULL_IFNULL_OBJ(obj, n) RETURN_IFNULL_OBJ(obj, NULL, n)



#define DeclareInstanceCleanup(pfx)                                          \
CMPIStatus  pfx##Cleanup(CMPIInstanceMI * mi, CMPIContext * ctx)

#define DeclareInstanceEnumInstanceNames(pfx)                                \
CMPIStatus  pfx##EnumInstanceNames(CMPIInstanceMI * mi, CMPIContext * ctx,   \
                                   CMPIResult * rslt, CMPIObjectPath * ref)

#define DeclareInstanceEnumInstances(pfx)                                    \
CMPIStatus  pfx##EnumInstances(CMPIInstanceMI * mi, CMPIContext* ctx,        \
                               CMPIResult * rslt, CMPIObjectPath * ref,      \
                               char ** properties)

#define DeclareInstanceGetInstance(pfx)                                      \
CMPIStatus  pfx##GetInstance(CMPIInstanceMI * mi,  CMPIContext * ctx,        \
                             CMPIResult * rslt, CMPIObjectPath * cop,        \
                             char ** properties)

#define DeclareInstanceCreateInstance(pfx)                                   \
CMPIStatus  pfx##CreateInstance(CMPIInstanceMI * mi, CMPIContext * ctx,      \
                                CMPIResult * rslt, CMPIObjectPath * cop,     \
                                CMPIInstance * ci)

#define DeclareInstanceSetInstance(pfx)                                      \
CMPIStatus  pfx##SetInstance(CMPIInstanceMI * mi, CMPIContext * ctx,         \
                             CMPIResult * rslt, CMPIObjectPath * cop,        \
                             CMPIInstance * ci, char ** properties)

#define DeclareInstanceDeleteInstance(pfx)                                   \
CMPIStatus pfx##DeleteInstance(CMPIInstanceMI * mi, CMPIContext * ctx,       \
                               CMPIResult * rslt, CMPIObjectPath * cop)

#define DeclareInstanceExecQuery(pfx)                                        \
CMPIStatus pfx##ExecQuery(CMPIInstanceMI * mi, CMPIContext * ctx,            \
                          CMPIResult * rslt, CMPIObjectPath * ref,           \
                          char * lang, char * query)

#define DeclareInstanceFunctions(pfx)           \
        static DeclareInstanceCleanup(pfx);            \
        static DeclareInstanceEnumInstanceNames(pfx);  \
        static DeclareInstanceEnumInstances(pfx);      \
        static DeclareInstanceGetInstance(pfx);        \
        static DeclareInstanceCreateInstance(pfx);     \
        static DeclareInstanceSetInstance(pfx);        \
        static DeclareInstanceDeleteInstance(pfx);     \
        static DeclareInstanceExecQuery(pfx)

#define DeclareMethodInvokeMethod(pfx)                                       \
CMPIStatus pfx##InvokeMethod(CMPIMethodMI * mi, CMPIContext * ctx,           \
                CMPIResult * rslt, CMPIObjectPath * ref,                     \
                const char * method, CMPIArgs * in, CMPIArgs * out)

#define DeclareMethodCleanup(pfx)                                            \
CMPIStatus pfx##MethodCleanup(CMPIMethodMI * mi, CMPIContext * ctx) 

#define DeclareMethodFunctions(pfx)             \
        static DeclareMethodInvokeMethod(pfx);         \
        static DeclareMethodCleanup(pfx)

#define DeclareAssociationCleanup(pfx)                                       \
CMPIStatus pfx##AssociationCleanup(CMPIAssociationMI * mi, CMPIContext * ctx) 

#define DeclareAssociationAssociators(pfx)                                   \
CMPIStatus pfx##Associators(CMPIAssociationMI * mi, CMPIContext * ctx,       \
                CMPIResult * rslt, CMPIObjectPath * op,                      \
                const char * asscClass, const char * resultClass,            \
                const char * role, const char * resultRole,                  \
                char ** properties)
#define DeclareAssociationAssociatorNames(pfx)                               \
CMPIStatus pfx##AssociatorNames(CMPIAssociationMI * mi, CMPIContext * ctx,   \
                CMPIResult * rslt, CMPIObjectPath * op,                      \
                const char * asscClass, const char * resultClass,            \
                const char * role, const char * resultRole)

#define DeclareAssociationReferences(pfx)                                    \
CMPIStatus pfx##References(CMPIAssociationMI * mi,                           \
                CMPIContext * ctx, CMPIResult * rslt,                        \
                CMPIObjectPath * op, const char * resultClass,               \
                const char * role, char ** properties)

#define DeclareAssociationReferenceNames(pfx)                                \
CMPIStatus pfx##ReferenceNames(CMPIAssociationMI * mi,                       \
                CMPIContext * ctx, CMPIResult * rslt, CMPIObjectPath * cop,  \
                const char * resultClass, const char * role)

#define DeclareAssociationFunctions(pfx)           \
        static DeclareAssociationCleanup(pfx);            \
        static DeclareAssociationAssociators(pfx);        \
        static DeclareAssociationAssociatorNames(pfx);    \
        static DeclareAssociationReferences(pfx);         \
        static DeclareAssociationReferenceNames(pfx)


#define DeclareIndicationCleanup(pfx)                                        \
CMPIStatus pfx##IndicationCleanup(CMPIIndicationMI * mi, CMPIContext * ctx)

#define DeclareIndicationAuthorizeFilter(pfx)                                \
CMPIStatus pfx##AuthorizeFilter(CMPIIndicationMI * mi, CMPIContext * ctx,    \
                           CMPIResult * rslt,                                \
                           CMPISelectExp * filter, const char * type,        \
                           CMPIObjectPath * classPath, const char * owner)

#define DeclareIndicationMustPoll(pfx)                                       \
CMPIStatus pfx##MustPoll(CMPIIndicationMI * mi,                              \
                CMPIContext * ctx, CMPIResult * rslt, CMPISelectExp * filter,\
                const char * indType, CMPIObjectPath * classPath)

#define DeclareIndicationActivateFilter(pfx)                                 \
CMPIStatus pfx##ActivateFilter(CMPIIndicationMI * mi,                        \
                CMPIContext * ctx, CMPIResult * rslt,                        \
                CMPISelectExp * filter, const char * type,                   \
                CMPIObjectPath * classPath, CMPIBoolean firstActivation)

#define DeclareIndicationDeActivateFilter(pfx)                               \
CMPIStatus pfx##DeActivateFilter(CMPIIndicationMI * mi,                      \
               CMPIContext * ctx, CMPIResult * rslt,                         \
               CMPISelectExp * filter, const char * type,                    \
               CMPIObjectPath * classPath, CMPIBoolean lastActivation)

#define DeclareIndicationFunctions(pfx)          \
        static DeclareIndicationCleanup(pfx);           \
        static DeclareIndicationAuthorizeFilter(pfx);   \
        static DeclareIndicationMustPoll(pfx);          \
        static DeclareIndicationActivateFilter(pfx);    \
        static DeclareIndicationDeActivateFilter(pfx)

#define DeclareInstanceMI(pfx, pn, broker)                             \
static char inst_provider_name [] = "instance"#pn;                     \
static CMPIInstanceMIFT instMIFT = {                                   \
        CMPICurrentVersion,                                            \
        CMPICurrentVersion,                                            \
        inst_provider_name,                                            \
        pfx##Cleanup,                                                  \
        pfx##EnumInstanceNames,                                        \
        pfx##EnumInstances,                                            \
        pfx##GetInstance,                                              \
        pfx##CreateInstance,                                           \
        pfx##SetInstance,                                              \
        pfx##DeleteInstance,                                           \
        pfx##ExecQuery                                                 \
};                                                                     \
CMPIInstanceMI *                                                       \
pn##_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx);          \
CMPIInstanceMI *                                                       \
pn##_Create_InstanceMI(CMPIBroker * brkr, CMPIContext * ctx)           \
{                                                                      \
        static CMPIInstanceMI mi = {                                   \
                NULL,                                                  \
                &instMIFT                                              \
        };                                                             \
        broker = brkr;                                                 \
        return &mi;                                                    \
}

#define DeclareMethodMI(pfx, pn, broker)                               \
static char method_provider_name [] = "method"#pn;                     \
static CMPIMethodMIFT methMIFT = {                                     \
        CMPICurrentVersion,                                            \
        CMPICurrentVersion,                                            \
        method_provider_name,                                          \
        pfx##MethodCleanup,                                            \
        pfx##InvokeMethod                                              \
};                                                                     \
CMPIMethodMI *                                                         \
pn##_Create_MethodMI(CMPIBroker * brkr, CMPIContext * ctx);            \
CMPIMethodMI *                                                         \
pn##_Create_MethodMI(CMPIBroker * brkr, CMPIContext * ctx) {           \
        static CMPIMethodMI mi = {                                     \
                NULL,                                                  \
                &methMIFT,                                             \
        };                                                             \
        broker=brkr;                                                   \
        return &mi;                                                    \
}


#define DeclareAssociationMI(pfx, pn, broker)                          \
static char assoc_provider_name [] = "association"#pn;                 \
static CMPIAssociationMIFT assocMIFT = {                               \
        CMPICurrentVersion,                                            \
        CMPICurrentVersion,                                            \
        assoc_provider_name,                                           \
        pfx##AssociationCleanup,                                       \
        pfx##Associators,                                              \
        pfx##AssociatorNames,                                          \
        pfx##References,                                               \
        pfx##ReferenceNames                                            \
};                                                                     \
CMPIAssociationMI *                                                    \
pn##_Create_AssociationMI(CMPIBroker * brkr, CMPIContext *ctx);        \
CMPIAssociationMI *                                                    \
pn##_Create_AssociationMI(CMPIBroker * brkr, CMPIContext *ctx)         \
{                                                                      \
        static CMPIAssociationMI mi = {                                \
                NULL,                                                  \
                &assocMIFT                                             \
        };                                                             \
        broker = brkr;                                                 \
        return &mi;                                                    \
}



#define DeclareIndicationMI(pfx, pn, broker)                           \
static char ind_provider_name [] = "Indication"#pn;                    \
static CMPIIndicationMIFT indMIFT = {                                  \
        CMPICurrentVersion,                                            \
        CMPICurrentVersion,                                            \
        ind_provider_name,                                             \
        pfx##IndicationCleanup,                                        \
        pfx##AuthorizeFilter,                                          \
        pfx##MustPoll,                                                 \
        pfx##ActivateFilter,                                           \
        pfx##DeActivateFilter,                                         \
        CMIndicationMIStubExtensions(pfx)                              \
};                                                                     \
CMPIIndicationMI *                                                     \
pn##_Create_IndicationMI(CMPIBroker * brkr, CMPIContext * ctx);        \
CMPIIndicationMI *                                                     \
pn##_Create_IndicationMI(CMPIBroker * brkr, CMPIContext * ctx) {       \
        static CMPIIndicationMI mi = {                                 \
                NULL,                                                  \
                &indMIFT,                                              \
        };                                                             \
        return &mi;                                                    \
}


#endif /* _CMPI_UTILS_H */
