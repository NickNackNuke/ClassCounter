package com.example.classcounter

import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener
import java.util.concurrent.TimeUnit

class HomePresenter : HomeContract.Presenter {
    private var view: HomeContract.View? = null
    private var counterListener: ValueEventListener? = null
    private val database = FirebaseDatabase.getInstance()
    private val counterRef = database.getReference("counter")

    override fun attach(view: HomeContract.View) { 
        this.view = view 
    }
    
    override fun detach() { 
        // Remove listener when view is detached
        counterListener?.let { counterRef.removeEventListener(it) }
        counterListener = null
        view = null 
    }

    override fun load() {
        // Show loading state
        view?.showStats("--", "--", "--")
        view?.showPeopleInside("--")
        view?.showLastUpdated("Connecting...")
        
        // Set up real-time listener for counter data
        counterListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                if (snapshot.exists()) {
                    val entryCount = snapshot.child("entryCount").getValue(Long::class.java) ?: 0
                    val exitCount = snapshot.child("exitCount").getValue(Long::class.java) ?: 0
                    val currentInRoom = snapshot.child("currentInRoom").getValue(Long::class.java) ?: 0
                    val lastUpdated = snapshot.child("lastUpdated").getValue(Long::class.java) ?: 0
                    
                    // Update UI
                    view?.showStats(
                        currentInRoom.toString(),
                        entryCount.toString(),
                        exitCount.toString()
                    )
                    view?.showPeopleInside(currentInRoom.toString())
                    view?.showLastUpdated(getTimeAgo(lastUpdated))
                } else {
                    // No data yet
                    view?.showStats("0", "0", "0")
                    view?.showPeopleInside("0")
                    view?.showLastUpdated("No data yet")
                }
            }

            override fun onCancelled(error: DatabaseError) {
                view?.showMessage("Error: ${error.message}")
                view?.showLastUpdated("Connection error")
            }
        }
        
        // Attach the listener
        counterRef.addValueEventListener(counterListener!!)
    }

    override fun onViewActivityLogClicked() {
        view?.showMessage("Activity Log clicked")
    }
    
    private fun getTimeAgo(timestamp: Long): String {
        if (timestamp == 0L) return "Never"
        
        val now = System.currentTimeMillis()
        val diff = now - timestamp
        
        return when {
            diff < TimeUnit.MINUTES.toMillis(1) -> "Updated just now"
            diff < TimeUnit.HOURS.toMillis(1) -> {
                val minutes = TimeUnit.MILLISECONDS.toMinutes(diff)
                "Updated ${minutes}m ago"
            }
            diff < TimeUnit.DAYS.toMillis(1) -> {
                val hours = TimeUnit.MILLISECONDS.toHours(diff)
                "Updated ${hours}h ago"
            }
            else -> {
                val days = TimeUnit.MILLISECONDS.toDays(diff)
                "Updated ${days}d ago"
            }
        }
    }
}
