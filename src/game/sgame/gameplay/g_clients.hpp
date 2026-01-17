#pragma once

#include <cstddef>
#include <new>

#include "../g_local.hpp"

// Utilities for constructing and destroying gclient_t ranges that live in
// TagMalloc-managed memory.  The constructors/destructors are run explicitly
// because the memory is obtained via placement new rather than new[].
inline void ConstructClients(gclient_t* clients, std::size_t count) {
        if (!clients)
                return;

        for (std::size_t i = 0; i < count; ++i)
                new (&clients[i]) gclient_t();
}

inline void DestroyClients(gclient_t* clients, std::size_t count) {
        if (!clients)
                return;

        for (std::size_t i = 0; i < count; ++i)
                clients[i].~gclient_t();
}

// Helper RAII wrapper that ensures client arrays constructed from external
// memory have their lifetime managed consistently.  The wrapper does not own
// the storage, it only guarantees the constructors/destructors are invoked in
// tandem with any reset operations.
class ClientArrayLifetime {
public:
        ClientArrayLifetime() = default;

        ClientArrayLifetime(gclient_t* clients, std::size_t count) {
                Reset(clients, count);
        }

        ~ClientArrayLifetime() {
                Reset(nullptr, 0);
        }

        void Reset(gclient_t* clients, std::size_t count) {
                if (clients_ && count_)
                        DestroyClients(clients_, count_);

                clients_ = clients;
                count_ = count;

                ConstructClients(clients_, count_);
        }

        gclient_t* Get() const {
                return clients_;
        }

        std::size_t Count() const {
                return count_;
        }

private:
        gclient_t* clients_ = nullptr;
        std::size_t count_ = 0;
};

// Allocate or reallocate the global client array and associated lag buffers.
// The helpers ensure constructor/destructor bookkeeping stays centralized and
// that dependent globals (e.g., globals.numEntities) are updated consistently.
void AllocateClientArray(int maxClients);
void FreeClientArray();
void ReplaceClientArray(int maxClients);

