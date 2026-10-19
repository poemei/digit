#ifndef DIGIT_CORE_ROUTER_H
#define DIGIT_CORE_ROUTER_H
/* Core message route table, not a conversation interpreter.
 * This preserves the current qualified Conversation service until
 * a module controller explicitly registers the generic route.
 * Routing is separate from permission and module qualification.
 */
#define DIGIT_MESSAGE_ROUTE "digit.conversation"
#endif
