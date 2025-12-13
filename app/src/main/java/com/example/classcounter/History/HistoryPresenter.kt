package com.example.classcounter.History

import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import java.util.Calendar

class HistoryPresenter : HistoryContract.Presenter {
    private var view: HistoryContract.View? = null
    private var historyListener: ValueEventListener? = null
    private var counterListener: ValueEventListener? = null
    private val database = FirebaseDatabase.getInstance()
    private val activityLogRef = database.getReference("activityLog")
    private val counterRef = database.getReference("counter")

    override fun attach(view: HistoryContract.View) {
        this.view = view
    }

    override fun detach() {
        historyListener?.let { activityLogRef.removeEventListener(it) }
        counterListener?.let { counterRef.removeEventListener(it) }
        historyListener = null
        counterListener = null
        view = null
    }

    override fun load() {
        view?.showLoading()

        // Listen for counter stats
        counterListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val insideNow = snapshot.child("currentInRoom").getValue(Long::class.java) ?: 0L
                val entryCount = snapshot.child("entryCount").getValue(Long::class.java) ?: 0L
                val exitCount = snapshot.child("exitCount").getValue(Long::class.java) ?: 0L
                view?.showStats(insideNow, entryCount, exitCount)
            }

            override fun onCancelled(error: DatabaseError) {
                // Silent fail for counter
            }
        }
        counterRef.addValueEventListener(counterListener!!)

        // Listen for history entries
        historyListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val entries = mutableListOf<HistoryEntry>()

                if (snapshot.exists()) {
                    for (child in snapshot.children) {
                        val type = child.child("type").getValue(String::class.java) ?: ""
                        val timestamp = child.child("timestamp").getValue(Long::class.java) ?: 0L
                        val count = child.child("count").getValue(Long::class.java) ?: 0L

                        entries.add(HistoryEntry(type, timestamp, count))
                    }

                    entries.sortByDescending { it.timestamp }
                }

                view?.showHistoryList(entries)
            }

            override fun onCancelled(error: DatabaseError) {
                view?.showError("Error: ${error.message}")
            }
        }

        activityLogRef.addValueEventListener(historyListener!!)
    }

    override fun clearHistory() {
        // Clear activity log
        activityLogRef.removeValue()
            .addOnSuccessListener {
                // Reset counter values to 0
                val resetData = mapOf(
                    "entryCount" to 0,
                    "exitCount" to 0,
                    "currentInRoom" to 0,
                    "lastUpdated" to System.currentTimeMillis()
                )
                
                counterRef.updateChildren(resetData)
                    .addOnSuccessListener {
                        view?.showSuccess("History cleared and counter reset to 0")
                    }
                    .addOnFailureListener { e ->
                        view?.showError("History cleared but failed to reset counter: ${e.message}")
                    }
            }
            .addOnFailureListener { e ->
                view?.showError("Failed to clear history: ${e.message}")
            }
    }
}
