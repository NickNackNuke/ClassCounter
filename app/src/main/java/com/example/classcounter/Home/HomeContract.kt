package com.example.classcounter

interface HomeContract {
    interface View {
        fun showStats(insideNow: String, enteredToday: String, exitedToday: String)
        fun showPeopleInside(count: String)
        fun showLastUpdated(text: String)
        fun showMessage(msg: String)
    }

    interface Presenter {
        fun attach(view: View)
        fun detach()
        fun load()
        fun onViewActivityLogClicked()
    }
}
