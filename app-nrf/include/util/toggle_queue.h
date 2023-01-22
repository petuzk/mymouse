/* Queue-like structure for storing toggle events. Stores up to UINT32_MAX (4'294'967'295) events.
 * The queue is based on two counters (head and tail). Since there are only two event types to be
 * stored (ON and OFF), and they must always alternate (i.e. it's not possible for an event to
 * occur twice in a row), the event type is actually stored in the counter's last bit.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

struct toggle_queue {
    uint32_t head, tail;
};

static inline void toggle_queue_init(struct toggle_queue* queue, bool initial_state) {
    queue->head = queue->tail = initial_state;
}

static inline int toggle_queue_len(struct toggle_queue* queue) {
    return queue->tail - queue->head;
}

static inline int toggle_queue_put(struct toggle_queue* queue, bool event) {
    if ((queue->head & 1) == event) {
        return -EINVAL;
    }
    ++queue->head;
    return event;
}

static inline int toggle_queue_get(struct toggle_queue* queue) {
    if (queue->tail == queue->head) {
        return -ENODATA;
    }
    return (++queue->tail) & 1;
}

static inline int toggle_queue_get_or_last(struct toggle_queue* queue) {
    if (queue->tail != queue->head) {
        ++queue->tail;
    }
    return queue->tail & 1;
}
