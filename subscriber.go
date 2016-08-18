package noblechild

import (
	"errors"
	"fmt"
	"sync"
)

type subscriber struct {
	sub map[uint16]subscribefn
	mu  *sync.Mutex
}

type subscribefn func([]byte, error)

func newSubscriber() *subscriber {

	s := &subscriber{
		sub: make(map[uint16]subscribefn),
		mu:  &sync.Mutex{},
	}

	return s
}

func (s *subscriber) subscribe(h uint16, f subscribefn) {
	if s == nil {
		fmt.Println("[subscribe] subscriber is nil. return.")
		return
	}
	s.mu.Lock()
	s.sub[h] = f
	s.mu.Unlock()
}

func (s *subscriber) unsubscribe(h uint16) {
	if s == nil {
		fmt.Println("[unsubscribe] subscriber is nil. return.")
		return
	}
	s.mu.Lock()
	delete(s.sub, h)
	s.mu.Unlock()
}

func (s *subscriber) fn(h uint16) subscribefn {
	if s == nil {
		fmt.Println("[fn] subscriber is nil. return nil.")
		return nil
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.sub[h]
}

var (
	ErrInvalidLength = errors.New("invalid length")
)
