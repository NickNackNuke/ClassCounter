package com.example.classcounter


import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import android.content.Intent
import android.widget.ImageButton
import androidx.fragment.app.Fragment

class HomeFragment : Fragment(), HomeContract.View {

    private val presenter: HomeContract.Presenter = HomePresenter()

    private lateinit var tvInsideNow: TextView
    private lateinit var tvEnteredToday: TextView
    private lateinit var tvExitedToday: TextView
    private lateinit var tvPeopleInside: TextView
    private lateinit var tvUpdated: TextView
    private lateinit var btnActivityLog: Button
    private lateinit var btnBack: ImageButton


    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.activity_home, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        tvInsideNow = view.findViewById(R.id.tvInsideNow)
        tvEnteredToday = view.findViewById(R.id.tvEnteredToday)
        tvExitedToday = view.findViewById(R.id.tvExitedToday)
        tvPeopleInside = view.findViewById(R.id.tvPeopleInside)
        tvUpdated = view.findViewById(R.id.tvUpdated)
        btnActivityLog = view.findViewById(R.id.btnActivityLog)
        btnActivityLog.setOnClickListener { presenter.onViewActivityLogClicked() }
        presenter.attach(this)
        presenter.load()
        btnBack = view.findViewById(R.id.btnBack)

       btnBack.setOnClickListener {
            val intent = Intent(requireContext(), Login::class.java)
            startActivity(intent)
            requireActivity().finish()
        }

        btnActivityLog.setOnClickListener {
            presenter.onViewActivityLogClicked()
        }
    }

    override fun onDestroyView() {
        presenter.detach()
        super.onDestroyView()
    }

    override fun showStats(insideNow: String, enteredToday: String, exitedToday: String) {
        tvInsideNow.text = insideNow
        tvEnteredToday.text = enteredToday
        tvExitedToday.text = exitedToday
    }
    override fun showPeopleInside(count: String) { tvPeopleInside.text = count }
    override fun showLastUpdated(text: String) { tvUpdated.text = text }
    override fun showMessage(msg: String) {
        Toast.makeText(requireContext(), msg, Toast.LENGTH_SHORT).show()
    }

    companion object { fun newInstance() = HomeFragment() }
}
