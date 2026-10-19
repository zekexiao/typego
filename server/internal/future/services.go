package future

import "context"

// UserService 预留用户系统边界，当前不提供实现。
type UserService interface {
	GetUser(context.Context, string) (any, error)
}

// SubscriptionService 预留订阅与付款边界，当前不提供实现。
type SubscriptionService interface {
	GetSubscription(context.Context, string) (any, error)
}
